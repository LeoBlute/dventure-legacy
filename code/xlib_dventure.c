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
   u64       SourceModfiedTime;
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
   struct stat FileStat;
   if(stat(Path, &FileStat) != 0) {
      return 0;
   }
   return FileStat.st_mtime;
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

static string_sequence XFilesOfTypeContent(char* Type) {
   string_sequence Result = {};
   return Result;
}

static char* ResourcesDirectoryPath = ".";

static u64 XFilesOfTypeTotalSizeForSequence(char* Extension, char* DirectoryPath) {
   DIR* Directory = opendir(DirectoryPath);
   return 0;
}

//Dot included
static u64 XFilesOfTypeFindChanged(char* Extension, char* DirectoryPath, u64 PreviousTime) {
   assert(Extension[0] == '.');
   u64 Result = PreviousTime;
   DIR* Directory = opendir(DirectoryPath);
   assert(Directory);
   struct dirent* Dirent = 0;
   while((Dirent = readdir(Directory))) {
      assert(Dirent);
      if(Dirent->d_type == DT_REG) {
         u32 ExtensionLen = strlen(Extension);
         u32 FileNameLen = strlen(Dirent->d_name);
         // printf("%s, %ld\n", dir->d_name, strlen(dir->d_name));
      }
   }
   return Result;
}

static u64 XFilesOfTypeChanged(char* Extension) {
   u64 ChangedTime = PLATFORM_NO_FILES;
   ChangedTime = XFilesOfTypeFindChanged(Extension, ".", ChangedTime);
   printf("%ld\n", ChangedTime);
   /*DIR* ResourcesDirectory;
   ResourcesDirectory = opendir(ResourcesDirectoryPath);
   if(!ResourcesDirectory) {
      printf("Error searching for the code directory\n");
      return 1;
   }
   struct dirent* dir = NULL;
   u64 DatesCount = 0;
   u64 LatestDate;

   while((dir = readdir(ResourcesDirectory)) != NULL) {
      // if(dir->d_type == DT_REG) {
         printf("%s, %d\n", dir->d_name, dir->d_type);
         ++DatesCount;
      // }
   }*/
   return 0;
}

int main() {
   assert(PAGES_TO_BYTES(1) == sysconf(_SC_PAGESIZE));
   char* CodeDirectoryPath = NULL;
#if defined(DVENTURE_CODE_PATH) && defined(DVENTURE_DEBUG)
   CodeDirectoryPath = DVENTURE_CODE_PATH;
#endif

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
      printf("Window wan't created properly\n");
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


   //TODO(LAG): Resolve absolute path
   char* GameDLPath = "/home/lag/Documents/Projects/dventure/build/libdventure.so";
   xgame_code GameCode = {};

   arena EternalArena = {};
   arena TransientArena = {};
   arena WorldArena = {};
   arena AssetArena = {};
   EternalArena.Size = PAGES_TO_BYTES(1);
   TransientArena.Size = PAGES_TO_BYTES(2);
   WorldArena.Size = PAGES_TO_BYTES(10);
   AssetArena.Size = PAGES_TO_BYTES(10);

   buffer TotalStorage = {};
   TotalStorage.Size = EternalArena.Size + TransientArena.Size + AssetArena.Size + WorldArena.Size;
   TotalStorage.Data = mmap(0, TotalStorage.Size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

   EternalArena.Data   = TotalStorage.Data;
   TransientArena.Data = TotalStorage.Data + EternalArena.Size;
   WorldArena.Data     = TotalStorage.Data + EternalArena.Size + TransientArena.Size + AssetArena.Size;
   AssetArena.Data     = TotalStorage.Data + EternalArena.Size + TransientArena.Size;

   b32 ShouldQuit = FALSE;
   app_state* AppState   = ArenaAlloc(&EternalArena, sizeof(app_state));
   game_input* GameInput = ArenaAlloc(&EternalArena, sizeof(game_input));
   platform_procedures* XProcedures = ArenaAlloc(&EternalArena, sizeof(platform_procedures));
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
                  /*char v = (char)symbol;
                  int numBits = 32; // Calculate the number of bits in the integer type
                  for (int i = numBits - 1; i >= 0; i--) {
                      // Extract and print each bit
                      printf("%d", (symbol >> i) & 1);
                  }
                  printf("\n");
                  printf("%d, %c\n", symbol, v);*/
               }
            } break;
         }
      }

      if(CodeDirectoryPath) {
         DIR* CodeDirectory;
         CodeDirectory = opendir(CodeDirectoryPath);
         if(!CodeDirectory) {
            printf("Error searching for the code directory\n");
            return 1;
         }
         struct dirent* dir = NULL;
         u64 DatesCount = 0;
         u64 LatestDate;

         while((dir = readdir(CodeDirectory)) != NULL) {
            if(dir->d_type == DT_REG) {
               ++DatesCount;
            }
         }

         //TODO: Problably don't need this array
         u64 DatesArray[DatesCount];
         u32 DatesIndex = 0;
         rewinddir(CodeDirectory);
         while((dir = readdir(CodeDirectory)) != NULL) {
            if(dir->d_type == DT_REG) {
               u64 PathLength = strlen(CodeDirectoryPath) + strlen(dir->d_name) + 2;
               char FilePath[PathLength];
               PathLength = sprintf(FilePath, "%s/%s", CodeDirectoryPath, dir->d_name);
               struct stat FileStats = {};
               if(stat(FilePath, &FileStats) != -1) {
                  DatesArray[DatesIndex++] = FileStats.st_mtime;
               } else {
                  return 1;
               }
            }
         }
         u64 CurrentFilesTime = 0;
         for(int i = 0; i < DatesCount; ++i) {
            if(DatesArray[i] > CurrentFilesTime) {
               CurrentFilesTime = DatesArray[i];
            }
         }
         if(GameCode.SourceModfiedTime != CurrentFilesTime) {
            GameCode.SourceModfiedTime = CurrentFilesTime;
            ShouldCompileCode = TRUE;
         }
         closedir(CodeDirectory);
      }
      
      glXSwapBuffers(XDisplay, XGLWindow);
      if(ShouldCompileCode) {
         assert(CodeDirectoryPath);
         printf("Compiling code...\n");
         int ret = system("gcc -O0 /home/lag/Documents/Projects/dventure/code/dventure.c -o /home/lag/Documents/Projects/dventure/build/libdventure.so -shared -fPIC -I/usr/include/GL");
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
      XProcedures->FilesOfTypeChanged = XFilesOfTypeChanged;
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
