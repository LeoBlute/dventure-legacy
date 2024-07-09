#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <dirent.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#define GLX_GLXEXT_PROTOTYPES
#include <GL/glx.h>

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "dventure.h"

typedef struct xgame_code {
   game_loop Loop;
   void*     DLHandle;
   u64       DLModfiedTime;
   u64       SourceModifiedTime;
} xgame_code;

static b32
XCreateGLContext(GLXWindow*  XGLWindow,
                 GLXContext* XGLContext,
                 Display*    XDisplay,
                 int         XScreen,
                 Window      XWindow) {
   int FDConfigAttrs[] = {
      GLX_DOUBLEBUFFER, TRUE,
      GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
      0
   };
   int GLXNFBConfigs;
   GLXFBConfig* GLXFBConfigs = glXChooseFBConfig(XDisplay, XScreen, FDConfigAttrs, &GLXNFBConfigs);
   if(!GLXNFBConfigs) {
      printf("Error on glXChooseFBConfig\n");
      return FALSE;
   }
   GLXFBConfig GLXFBConfig = GLXFBConfigs[0];
   XFree(GLXFBConfigs);
   int GLXVisualID;
   if(glXGetFBConfigAttrib(XDisplay, GLXFBConfig, GLX_VISUAL_ID, &GLXVisualID) > 0) {
      printf("Error on glxGetFBConfigAttrib\n");
      return FALSE;
   }

   *XGLWindow = glXCreateWindow(XDisplay, GLXFBConfig, XWindow, NULL);
   *XGLContext = glXCreateNewContext(XDisplay, GLXFBConfig, GLX_RGBA_TYPE, 0, True);
   glXMakeContextCurrent(XDisplay, *XGLWindow, *XGLWindow, *XGLContext);

   return TRUE;
}

static void XUnloadGameCode(xgame_code* GameCode) {
   if(GameCode->DLHandle) {
      game_unload GameUnload = dlsym(GameCode->DLHandle, "GameUnload");
      assert(GameUnload);
      GameUnload();

      dlclose(GameCode->DLHandle);
      GameCode->DLHandle = 0;
   }
   GameCode->Loop = NULL;
}

static void XLoadGameCode(char* DLPath, xgame_code* GameCode) {
   GameCode->DLHandle = dlopen(DLPath, RTLD_LAZY);
   GameCode->Loop = dlsym(GameCode->DLHandle, "GameLoop");
   if(!GameCode->Loop || !GameCode->DLHandle) {
      XUnloadGameCode(GameCode);
   }
}

static u64 XGetFileTime(char* Path) {
   u64 Result = 0;
   struct stat FileStat;
   if(stat(Path, &FileStat) != 0) {
      return 0;
   }
   if(Result < FileStat.st_atime) {
      Result = FileStat.st_atime;
   }
   if(Result < FileStat.st_mtime) {
      Result = FileStat.st_mtime;
   }
   if(Result < FileStat.st_ctime) {
      Result = FileStat.st_ctime;
   }
   return Result;
}

static file_data XGetFileData(char* Path) {
   file_data Result = {};
   struct stat FileStat;
   if(stat(Path, &FileStat) != 0) {
      return Result;
   }
   if(Result.ChangedID < FileStat.st_atime) {
      Result.ChangedID = FileStat.st_atime;
   }
   if(Result.ChangedID < FileStat.st_mtime) {
      Result.ChangedID = FileStat.st_mtime;
   }
   if(Result.ChangedID < FileStat.st_ctime) {
      Result.ChangedID = FileStat.st_ctime;
   }
   Result.ContentSize = FileStat.st_size;
   return Result;
}

//TODO: Make this work on doubles
/*static u64 XCurrentTime() {
   struct timeval Timeval;
   gettimeofday(&Timeval, NULL);
   return (Timeval.tv_sec * 1000000LLU) + Timeval.tv_usec;
}*/

static void* XAllocate(u64 Size) {
   return mmap(0, Size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);;
}

static void XDeallocate(void* Memory, u64 Size) {
   munmap(Memory, Size);
}

static b8 XIsFileOfType(char* FileName, char* Extension) {
   assert(FileName && Extension);
   u32 FileNameLen = strlen(FileName);
   u32 ExtensionLen = strlen(Extension);
   if(FileNameLen <= ExtensionLen) {
      return FALSE;
   }

   for(u32 i = (FileNameLen - ExtensionLen), j = 0; i < FileNameLen; ++i, ++j) {
      if(FileName[i] != Extension[j]) {
         return FALSE;
      }
   }

   return TRUE;
}

static char* ResourcesDirectoryPath = ".";

static u64 XFilesOfTypeGetContent(char* Extension, buffer* FillSequence, char* DirectoryPath, u64 PreviousBytesCommited) {
   assert(Extension[0] == '.');
   u64 BytesCommited = 0;
   DIR* Directory;
   struct dirent* Dirent;

   Directory = opendir(DirectoryPath);
   assert(Directory);

   while((Dirent = readdir(Directory))) {
      assert(Dirent);
      if(Dirent->d_type == DT_REG && XIsFileOfType(Dirent->d_name, Extension)) {
         char* FilePath = NULL;
         //TODO: Remove this allocation
         FilePath = malloc(strlen(DirectoryPath) + strlen(Dirent->d_name) + 2);
         strcpy(FilePath, DirectoryPath);
         strcat(FilePath, "/");
         strcat(FilePath, Dirent->d_name);

         int FileHandle = open(FilePath, O_RDONLY);
         assert(FileHandle != -1);

         struct stat FileStats = {};
         assert(fstat(FileHandle, &FileStats) != -1);

         u64 FileContentSize = FileStats.st_size;
         u64 OffsetForNextContent = sizeof(u64) + strlen(FilePath) + 1 + FileStats.st_size;
         u64* U64Ptr = (u64*)(FillSequence->Data + BytesCommited);
         *U64Ptr = OffsetForNextContent;
         BytesCommited += sizeof(u64);

         //TODO: Clean this
         memcpy(FillSequence->Data + BytesCommited, FilePath, strlen(FilePath));
         BytesCommited += strlen(FilePath);
         char* Ptr = (char*)(FillSequence->Data + BytesCommited);
         *Ptr = '\0';
         BytesCommited++;

         char* FileContent = (char*)mmap(0, FileStats.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, FileHandle, 0);
         memcpy(FillSequence->Data + BytesCommited, FileContent, FileStats.st_size);
         BytesCommited += FileStats.st_size;

         close(FileHandle);
         munmap(FileContent, FileStats.st_size);
         free(FilePath);
      } else if(Dirent->d_type == DT_DIR && strcmp(Dirent->d_name, ".") != 0 && strcmp(Dirent->d_name, "..") != 0) {
         char* DirPath = NULL;
         DirPath = malloc(strlen(Dirent->d_name) + strlen(DirectoryPath) + 2);
         strcpy(DirPath, DirectoryPath);
         strcat(DirPath, "/");
         strcat(DirPath, Dirent->d_name);
         BytesCommited = XFilesOfTypeGetContent(Extension, FillSequence, DirPath, BytesCommited);
         free(DirPath);
      }
   }

   closedir(Directory);
   return BytesCommited;
}

static void XFilesOfTypeContent(char* Extension, buffer* FillSequence) {
   u64 BytesCommited = 0;
   BytesCommited = XFilesOfTypeGetContent(Extension, FillSequence, ResourcesDirectoryPath, BytesCommited);
}

//Dot included
//Refactor: Anything related to time must be chnaged
static file_data XFilesOfTypeFindChanged(char* Extension, char* DirectoryPath, file_data PreviousTime) {
   assert(Extension[0] == '.');

   file_data Result = PreviousTime;
   DIR* Directory;
   struct dirent* Dirent;

   Directory = opendir(DirectoryPath);
   assert(Directory);

   while((Dirent = readdir(Directory))) {
      if(Dirent->d_type == DT_REG && XIsFileOfType(Dirent->d_name, Extension)) {
         u32 FileNameLen = strlen(Dirent->d_name);
         u32 DirectoryPathLen = strlen(DirectoryPath);

         char FilePath[DirectoryPathLen + FileNameLen + 2];
         sprintf(FilePath, "%s/%s", DirectoryPath, Dirent->d_name);

         const file_data FileData = XGetFileData(FilePath);
         Result.ChangedID += FileData.ChangedID;
         Result.ContentSize += FileData.ContentSize + (strlen(FilePath) + 1) + sizeof(u64);
      }
      else if(Dirent->d_type == DT_DIR &&
         strcmp(Dirent->d_name, ".") != 0 &&
         strcmp(Dirent->d_name, "..") != 0
      ) {
         char DirPath[strlen(Dirent->d_name) + strlen(DirectoryPath) + 2];
         sprintf(DirPath, "%s/%s", DirectoryPath, Dirent->d_name);
         Result = XFilesOfTypeFindChanged(Extension, DirPath, Result);
      }
   }
   closedir(Directory);

   return Result;
}

static file_data XFilesOfTypeData(char* Extension) {
   file_data Result = {};
   Result = XFilesOfTypeFindChanged(Extension, ".", Result);
   return Result;
}

static u64 XSizeForAppendPath(char* Path1, char* Path2) {
   return strlen(Path1) + strlen(Path2) + 2;
}

static void XAppendPath(char* Buffer, char* Path1, char* Path2) {
   sprintf(Buffer, "%s/%s", Path1, Path2);
}

static u64 XFilesTypeModifiedIdRecursiveSearch(char* Extension, char* DirectoryPath) {
   u64 Id = 0;

   DIR* Directory;
   struct dirent* Dirent;

   Directory = opendir(DirectoryPath);
   assert(Directory);
   while((Dirent = readdir(Directory))) {
      assert(Dirent);
      if(Dirent->d_type == DT_REG && XIsFileOfType(Dirent->d_name, Extension)) {
         struct stat FileStat;
         u64 ModifiedId;

         char FilePath[XSizeForAppendPath(DirectoryPath, Dirent->d_name)];
         XAppendPath(FilePath, DirectoryPath, Dirent->d_name);

         int Result = stat(FilePath, &FileStat);
         assert(Result == 0);

         ModifiedId = FileStat.st_atime + FileStat.st_mtime + FileStat.st_ctime;
         Id += ModifiedId;
      }
      else if(Dirent->d_type == DT_DIR && strcmp(Dirent->d_name, ".") != 0 && strcmp(Dirent->d_name, "..") != 0){
         char DirPath[XSizeForAppendPath(DirectoryPath, Dirent->d_name)];
         XAppendPath(DirPath, DirectoryPath, Dirent->d_name);

         Id += XFilesTypeModifiedIdRecursiveSearch(Extension, DirPath);
      }
   }
   closedir(Directory);

   return Id;
}

static u64 XFilesTypeModifiedId(char* Extension) {
   u64 Id = XFilesTypeModifiedIdRecursiveSearch(Extension, ".");
   return Id;
}

//TODO(LAG); Refactor this? it's not in critical situation, so you may just ignore it
static b8 XFilesTypeIterateRecursive(char* DirectoryPath, char* Extension, _file_data* Data, b8* IsNextFile) {
   DIR* Directory;
   struct dirent* Dirent;

   // b8 IsNextFile = (Data->name == NULL);
   b8 Result = FALSE;

   Directory = opendir(DirectoryPath);
   assert(Directory);
   while((Dirent = readdir(Directory))) {
      assert(Dirent);
      if(Dirent->d_type == DT_REG && XIsFileOfType(Dirent->d_name, Extension)) {
         char FilePath[XSizeForAppendPath(DirectoryPath, Dirent->d_name)];
         XAppendPath(FilePath, DirectoryPath, Dirent->d_name);

         //Todo(LAG): Refactor this to become more readable
         b8 IsLastIteratedFile = Data->name && strcmp(Data->name, FilePath) == 0;
         if(IsLastIteratedFile) {
            *IsNextFile = TRUE;
         }

         if((*IsNextFile && !IsLastIteratedFile) || !Data->name) {
            if(Data->name) {
               free(Data->name);
               Data->name = NULL;
            }
            if(Data->content) {
               munmap(Data->content, Data->content_size);
               Data->content = NULL;
            }
            Data->name = malloc(XSizeForAppendPath(DirectoryPath, Dirent->d_name));
            XAppendPath(Data->name, DirectoryPath, Dirent->d_name);

            int FileHandle = open(Data->name, O_RDONLY);
            struct stat FileStat;

            int StatResult = fstat(FileHandle, &FileStat);
            assert(StatResult == 0);

            Data->content_size = FileStat.st_size;
            Data->content = mmap(NULL, Data->content_size, PROT_WRITE, MAP_PRIVATE, FileHandle, 0);
            Data->modified_id = FileStat.st_atime + FileStat.st_mtime + FileStat.st_ctime;

            close(FileHandle);

            Result = TRUE;
            break;
         }
      } else if(Dirent->d_type == DT_DIR && strcmp(Dirent->d_name, ".") != 0 && strcmp(Dirent->d_name, "..") != 0){
         char DirPath[XSizeForAppendPath(DirectoryPath, Dirent->d_name)];
         XAppendPath(DirPath, DirectoryPath, Dirent->d_name);

         if(XFilesTypeIterateRecursive(DirPath, Extension, Data, IsNextFile)) {
            Result = TRUE;
            break;
         }
      }
   }

   closedir(Directory);
   return Result;
}

static b8 XFilesTypeIterate(char* Extension, _file_data* Data) {
   b8 IsNextFile = FALSE;
   if (XFilesTypeIterateRecursive("./resources", Extension, Data, &IsNextFile)) {
      return TRUE;
   }

   if(Data->name) {
      free(Data->name);
   }
   if(Data->content) {
      munmap(Data->content, Data->content_size);
   }

   return FALSE;
}

int main() {
   XFilesTypeModifiedId(".ttf");
   for(_file_data Data = {}; XFilesTypeIterate(".ttf", &Data);) {
      if(Data.name == NULL) {
         printf("(NULL)\n");
      } else {
         printf("%s\n", Data.name);
      }
   }
   assert(PAGES_TO_BYTES(1) == sysconf(_SC_PAGESIZE));

   Display* XDisplay;
   int      XRoot;
   int      XScreen;
   int      XScreenBitDepth;
   XVisualInfo XVisualInfo;
   XSetWindowAttributes XSetWindowAttributes;
   u32 XAttributeMask;
   Window XWindow;

   if(!(XDisplay = XOpenDisplay(NULL))) {
      printf("No Display Available\n");
      return 1;
   }
   XRoot = DefaultRootWindow(XDisplay);
   XScreen = DefaultScreen(XDisplay);
   XScreenBitDepth = 24;
   if(!XMatchVisualInfo(XDisplay, XScreen, XScreenBitDepth, TrueColor, &XVisualInfo)) {
      printf("No matching visual info\n");
      return 1;
   }

   XSetWindowAttributes.background_pixel = 0;
   XSetWindowAttributes.colormap = XCreateColormap(XDisplay, XRoot, XVisualInfo.visual, AllocNone);
   XSetWindowAttributes.event_mask = KeyPressMask | KeyReleaseMask;
   XAttributeMask = CWBackPixel | CWColormap | CWEventMask;

   XWindow = XCreateWindow(XDisplay,
                              XRoot,
                              0, 0,
                              800, 600,
                              0,
                              XVisualInfo.depth,
                              InputOutput,
                              XVisualInfo.visual,
                              XAttributeMask, &XSetWindowAttributes);

   if(!XWindow) {
      printf("Window wasn't created properly\n");
      return 1;
   }

   XStoreName(XDisplay, XWindow, "DVenture");
   XMapWindow(XDisplay, XWindow);
   XFlush(XDisplay);

   Atom WM_DELETE_WINDOW = XInternAtom(XDisplay, "WM_DELETE_WINDOW", FALSE);
   if(!XSetWMProtocols(XDisplay, XWindow, &WM_DELETE_WINDOW, 1)) {
      printf("Couldn't register WM_DELETE_WINDOW property\n");
      return 1;
   }

   XkbSetDetectableAutoRepeat(XDisplay, TRUE, NULL);

   GLXWindow XGLWindow;
   GLXContext XGLContext;
   if(!XCreateGLContext(&XGLWindow, &XGLContext, XDisplay, XScreen, XWindow)) {
      return 1;
   }

   XIM XInputMethod = XOpenIM(XDisplay, 0, 0, 0);
   if(!XInputMethod) {
      printf("Input Method could not be opened\n");
   }

   XIMStyles* XInputMethodStyles = 0;
   if(XGetIMValues(XInputMethod, XNQueryInputStyle, &XInputMethodStyles, NULL) || !XInputMethodStyles) {
      printf("Input Styles could not be retrieved\n");
   }

   XIMStyle XBestMatchStyle = 0;
   for(int i=0; i < XInputMethodStyles->count_styles; i++)
   {
      XIMStyle XThisStyle = XInputMethodStyles->supported_styles[i];
      if (XThisStyle == (XIMPreeditNothing | XIMStatusNothing)) {
         XBestMatchStyle = XThisStyle;
         break;
      }
   }
   XFree(XInputMethodStyles);

   if(!XBestMatchStyle) {
      printf("No matching input style could be determined\n");
   }

   XIC XInputContext = XCreateIC(XInputMethod, XNInputStyle, XBestMatchStyle,
                                 XNClientWindow, XWindow,
                                 XNFocusWindow,  XWindow,
                                 NULL);
   if(!XInputContext) {
      printf("Input Context could not be created\n");
   }

   char CodeDirectoryPath[512];
   char AbsolutePath[512] = {};
   char GameDLPath[512] = {};
   {
      readlink("/proc/self/exe", AbsolutePath, sizeof(AbsolutePath));
      char* PathEnd = AbsolutePath + strlen(AbsolutePath);

      //Cut the execute "dventure" from the absolute path so that we have the absolute directory path
      while(TRUE) {
         *PathEnd = 0;
         PathEnd--;
         if(*PathEnd == '/') {
            *PathEnd = 0;
            break;
         }
      }

      #pragma GCC diagnostic push
      #pragma GCC diagnostic ignored "-Wformat-overflow"
      sprintf(GameDLPath, "%s/libdventure.so", AbsolutePath);
      sprintf(CodeDirectoryPath, "%s/source", AbsolutePath);
      #pragma GCC diagnostic pop
   }

   xgame_code GameCode = {};

   arena EternalArena = {};
   arena TransientArena = {};
   arena WorldArena = {};
   arena AssetArena = {};
   EternalArena.Size = PAGES_TO_BYTES(1);
   TransientArena.Size = PAGES_TO_BYTES(2);
   WorldArena.Size = PAGES_TO_BYTES(10);
   AssetArena.Size = PAGES_TO_BYTES(5000);

   buffer TotalStorage = {};
   TotalStorage.Size = EternalArena.Size + TransientArena.Size + AssetArena.Size + WorldArena.Size;
   TotalStorage.Data = mmap(0, TotalStorage.Size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

   EternalArena.Data   = TotalStorage.Data;
   TransientArena.Data = TotalStorage.Data + EternalArena.Size;
   WorldArena.Data     = TotalStorage.Data + EternalArena.Size + TransientArena.Size + AssetArena.Size;
   AssetArena.Data     = TotalStorage.Data + EternalArena.Size + TransientArena.Size;

   app_state* AppState   = ArenaAlloc(&EternalArena, sizeof(app_state));
   game_input* GameInput = ArenaAlloc(&EternalArena, sizeof(game_input));
   platform_procedures* XProcedures = ArenaAlloc(&EternalArena, sizeof(platform_procedures));

   b32 ShouldQuit = FALSE;
   while(!ShouldQuit) {
      CARRAY_ZERO(GameInput->Characters);
      b32 Initialized = ((app_state*)(EternalArena.Data))->Initialized;
      b32 ShouldCompileCode = FALSE;
      while(XPending(XDisplay)) {
         XEvent Event;
         XNextEvent(XDisplay, &Event);
         switch(Event.type) {
            case ClientMessage:
            {
               XClientMessageEvent* e =(XClientMessageEvent*)&Event;
               if((Atom)e->data.l[0] == WM_DELETE_WINDOW) {
                  ShouldQuit = TRUE;
               }
            } break;
            case KeyRelease:
            case KeyPress:
            {
               XKeyPressedEvent* XKeyEvent = (XKeyPressedEvent*)&Event;
               b8 KeyDown  = Event.type == KeyPress ? TRUE : FALSE;
               u32 Keycode = Event.xkey.keycode;
               // printf("KC:%u\n", Keycode);
               if(Keycode == 25) {
                  GameInput->Up = KeyDown;
               } else if(Keycode == 39) {
                  GameInput->Down = KeyDown;
               } else if(Keycode == 38) {
                  GameInput->Left = KeyDown;
               } else if(Keycode == 40) {
                  GameInput->Right = KeyDown;
               }

               int symbol = 0;
               Status status = 0;
               Xutf8LookupString(XInputContext, XKeyEvent, (char*)&symbol, 4, 0, &status);

               if(status == XBufferOverflow)
               {
                  // Should not happen since there are no utf-8 characters larger than 24bits
                  // But something to be aware of when used to directly write to a string buffer
                  printf("Buffer overflow when trying to create keyboard symbol map\n");
               } else if(status == XLookupChars) {
                  char v = (char)symbol;
                  CARRAY_INSERT(GameInput->Characters, v);
               }
            } break;
         }
      }

      //Check if game code needs to be reloaded
      if(CodeDirectoryPath) {
         DIR* CodeDirectory;
         struct dirent* Dirent;
         u64 CurrentFilesTime;

         CodeDirectory = opendir(CodeDirectoryPath);
         if(!CodeDirectory) {
            printf("Error searching for the code directory\n");
            return 1;
         }

         CurrentFilesTime = 0;
         while((Dirent = readdir(CodeDirectory)) != NULL) {
            if(Dirent->d_type == DT_REG) {
               char FilePath[1024];
               sprintf(FilePath, "%s/%s", CodeDirectoryPath, Dirent->d_name);

               struct stat FileStats = {};
               if(stat(FilePath, &FileStats) != -1) {
                  if(FileStats.st_mtime > CurrentFilesTime) {
                     CurrentFilesTime = FileStats.st_mtime;
                  }
               } else {
                  printf("Error! Could not get Shared Library file stats\n");
                  return 1;
               }
            }
         }
         closedir(CodeDirectory);

         if(GameCode.SourceModifiedTime != CurrentFilesTime) {
            GameCode.SourceModifiedTime = CurrentFilesTime;
            ShouldCompileCode = TRUE;
         }
      }

      glXSwapBuffers(XDisplay, XGLWindow);

      if(ShouldCompileCode) {
         assert(CodeDirectoryPath);
         printf("Compiling code...\n");
         char CompileCommand[2048];
         sprintf(CompileCommand, "gcc -O0 %s/dventure.c -o %s/libdventure.so -shared -fPIC -I/usr/include/GL", CodeDirectoryPath, AbsolutePath);
         int Return = system(CompileCommand);
      }

      u64 CurrentDLTime = XGetFileTime(GameDLPath);
      if(CurrentDLTime != GameCode.DLModfiedTime) {
         if(CurrentDLTime == 0) {
            printf("Code Loading Failed\n");
         } else {
            XUnloadGameCode(&GameCode);
            XLoadGameCode(GameDLPath, &GameCode);
            if(GameCode.Loop) {
               printf("Code Loaded Successfully\n");
            } else {
               printf("Code Loading Failed\n");
            }
         }
         GameCode.DLModfiedTime = CurrentDLTime;
      }

      if(!GameCode.Loop) {
         printf("No Game Code Available\n");
         return 1;
      }

      XWindowAttributes XWindowAttributes = {};
      if(XGetWindowAttributes(XDisplay, XWindow, &XWindowAttributes) == 0) {
         printf("Could not get Window Attributes\n");
         return 1;
      }

      AppState->Width  = XWindowAttributes.width;
      AppState->Height = XWindowAttributes.height;
      XProcedures->Allocate = XAllocate;
      XProcedures->Deallocate = XDeallocate;
      XProcedures->FilesOfTypeContent = XFilesOfTypeContent;
      XProcedures->FilesOfTypeData = XFilesOfTypeData;

      game_context GameContext = {};
      GameContext.AppState = AppState;
      GameContext.Input    = GameInput;
      GameContext.Platform = XProcedures;
      GameContext.TransientArena = &TransientArena;
      GameContext.WorldArena     = &WorldArena;
      GameContext.AssetArena     = &AssetArena;

      GameCode.Loop(&GameContext);
   }

   //TODO(LAG) Consider if it is worth deallocating all the memory here or let the OS do all the work
}
