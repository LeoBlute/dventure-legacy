#include "dventure.h"
// #define STB_IMAGE_IMPLEMENTATION
// #define STBI_NO_STDIO
#define STB_TRUETYPE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_truetype.h"
#include "stb_image_write.h"
// #include "stb_image.h"

#include <stdio.h>
#include <gl.h>

typedef struct game_state {
   u32 AspectRatioHeight;
   u32 AspectRatioWidth;
   u32 Width;
   u32 Height;
   f32 CameraView;
   file_data ChangedTTFData;
   u32 TextureHandle;
   struct {
      u32 GlyphCount;
      u32 Width;
      u32 Height;
      stbtt_bakedchar* CharData;
      char* Bitmap;
      f32 Size;
   } FontData;
   struct {
      b8 Active;
      char Text[16];
   } Console;
} game_state;

static game_context* GlobalContext;
static game_state    GlobalState;

static void* TransientAlloc(u64 Size) {
   ArenaAlloc(GlobalContext->TransientArena, Size);
}

static rect32 FitDisplayAreaToAspectRatio(s32 AspectRatioWidth, s32 AspectRatioHeight, s32 AppWidth, s32 AppHeight) {
   rect32 Result = {};
   f32 OptimalWidth  = (f32)AppHeight  * ((f32)AspectRatioWidth  / (f32)AspectRatioHeight);
   f32 OptimalHeight = (f32)AppWidth   * ((f32)AspectRatioHeight / (f32)AspectRatioWidth);
   if(OptimalWidth > (f32)AppWidth) {
      Result.MinX = 0;
      Result.MaxX = AppWidth;

      f32 DeadOffset = (f32)AppHeight - OptimalHeight;
      s32 HalfDeadOffset = (s32)(0.5f * DeadOffset);
      s32 UseHeight = (s32)(OptimalHeight);

      Result.MinY = HalfDeadOffset;
      Result.MaxY = Result.MinY + UseHeight;
   } else {
      Result.MinY = 0;
      Result.MaxY = AppHeight;

      f32 DeadOffset = (f32)AppWidth - OptimalWidth;
      s32 HalfDeadOffset = (s32)(0.5f * DeadOffset);
      s32 UseWidth = (s32)(OptimalWidth);

      Result.MinX = HalfDeadOffset;
      Result.MaxX = Result.MinX + UseWidth;
   }
   return Result;
}

static frect32 WorldToDisplayRect(frect32 Rect) {
   f32 AspectFraction = (f32)GlobalState.AspectRatioWidth / (f32)GlobalState.AspectRatioHeight;
   f32 InversedCameraView = 1.0f / GlobalState.CameraView;
   f32 DisplayXFactor = InversedCameraView;
   f32 DisplayYFactor = AspectFraction * InversedCameraView;
   frect32 Result;
   Result.W = Rect.W * DisplayXFactor;
   Result.H = Rect.H * DisplayYFactor;
   Result.X = Rect.X * DisplayXFactor;
   Result.Y = Rect.Y * DisplayYFactor;
   return Result;
}

static void DrawRectFromWorld(frect32 Rect) {
   frect32 DisplayRect = WorldToDisplayRect(Rect);
   f32 HW = DisplayRect.W * 0.5f;
   f32 HH = DisplayRect.H * 0.5f;
   f32 X = DisplayRect.X;
   f32 Y = DisplayRect.Y;

   glEnable(GL_TEXTURE_2D);
   glBindTexture(GL_TEXTURE_2D, GlobalState.TextureHandle);

   glBegin(GL_TRIANGLE_STRIP);
   glTexCoord2f(0.0f, 0.0f); glVertex2f( X - HW, Y - HH); //Bottom Left
   glTexCoord2f(0.0f, 1.0f); glVertex2f( X - HW, Y + HH); //Top Left
   glTexCoord2f(1.0f, 0.0f); glVertex2f( X + HW, Y - HH); //Bottom Right
   glTexCoord2f(1.0f, 1.0f); glVertex2f( X + HW, Y + HH); //Top Right
   glEnd();
}

static void DrawText(char* Text, float X, float Y, float Size) {
   char* TextIterator = Text;
   u32 FontWidth = GlobalState.FontData.Width;
   u32 FontHeight = GlobalState.FontData.Height;
   f32 Scalator = 10.0f * GlobalState.FontData.Size;
   X = -Scalator;
   Y = -Scalator + (Scalator / 8.0f);

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glEnable(GL_TEXTURE_2D);
   glBindTexture(GL_TEXTURE_2D, GlobalState.TextureHandle);
   glBegin(GL_QUADS);
   while(*TextIterator) {
      if(*TextIterator >= 32 && *TextIterator < 128) {
         stbtt_aligned_quad q;
         stbtt_GetBakedQuad(GlobalState.FontData.CharData, FontWidth, FontHeight, *TextIterator-32, &X, &Y, &q, 1);

         f32 x0 = q.x0 / Scalator;
         f32 x1 = q.x1 / Scalator;
         f32 y0 = q.y0 / Scalator;
         f32 y1 = q.y1 / Scalator;
         glTexCoord2f(q.s0, q.t0); glVertex2f(x0, -y0);
         glTexCoord2f(q.s1, q.t0); glVertex2f(x1, -y0);
         glTexCoord2f(q.s1, q.t1); glVertex2f(x1, -y1);
         glTexCoord2f(q.s0, q.t1); glVertex2f(x0, -y1);
      }
      TextIterator++;
   }
   glEnd();
}

static f32 TemporalX;
static f32 TemporalY;

FORCE_INLINE u64 ContentOffset(char* Content) {
   return (*(u64*)(Content));
}

static buffer ContentData(buffer* ContentsBuffer, u64 Offset) {
   buffer Result = {};
   char* ContentNameBegin = NULL;
   u64 ContentOffsetForNext = 0;
   u64 ContentNameLen = 0;
   ContentOffsetForNext = ContentOffset(ContentsBuffer->Data + Offset);
   ContentNameBegin = (ContentsBuffer->Data + Offset + sizeof(u64));
   ContentNameLen = strlen(ContentNameBegin);
   Result.Data = ContentNameBegin + ContentNameLen + 1;
   Result.Size = ContentOffsetForNext - ContentNameLen - 1 - sizeof(u64);
   return Result;
}

void GameLoop(game_context* Context) {
   assert(Context->AppState && Context->Input &&
          Context->TransientArena && Context->WorldArena && Context->AssetArena);
   assert(Context->Platform->Allocate && Context->Platform->Deallocate && Context->Platform->FilesOfTypeContent && Context->Platform->FilesOfTypeData);
   GlobalContext = Context;

   if(!Context->AppState->Initialized) {
      GlobalContext->AppState->Initialized = TRUE;
   } else {
      CARRAY_FOR_EACH(C, GlobalContext->Input->Characters) {
         // printf("%c\n", C);
      }
      file_data ChangedTTFData = GlobalContext->Platform->FilesOfTypeData(".ttf");
      if(FILE_DATA_CHANGED(GlobalState.ChangedTTFData, ChangedTTFData)) {
         ArenaReset(GlobalContext->AssetArena);

         GlobalState.ChangedTTFData =  ChangedTTFData;
         buffer FillToSequence = {};

         FillToSequence.Data = ArenaAlloc(GlobalContext->AssetArena, GlobalState.ChangedTTFData.ContentSize);
         FillToSequence.Size = GlobalState.ChangedTTFData.ContentSize;
         GlobalContext->Platform->FilesOfTypeContent(".ttf", &FillToSequence);

         printf("PGChanged, %ld\n", GlobalState.ChangedTTFData.ContentSize);

         buffer RawTTFData = ContentData(&FillToSequence, ContentOffset(FillToSequence.Data));

         GlobalState.FontData.Width  = 1024;
         GlobalState.FontData.Height = 1024;
         GlobalState.FontData.GlyphCount = 96;
         GlobalState.FontData.Size = 96.0f;
         GlobalState.FontData.Bitmap = ArenaAlloc(GlobalContext->AssetArena, GlobalState.FontData.Width * GlobalState.FontData.Height);
         GlobalState.FontData.CharData = ArenaAlloc(GlobalContext->AssetArena, GlobalState.FontData.GlyphCount * sizeof(stbtt_bakedchar));
         stbtt_BakeFontBitmap(RawTTFData.Data,
                              0,
                              GlobalState.FontData.Size,
                              GlobalState.FontData.Bitmap,
                              GlobalState.FontData.Width,
                              GlobalState.FontData.Height,
                              32,
                              GlobalState.FontData.GlyphCount,
                              GlobalState.FontData.CharData);

         GLuint Texture;
         glGenTextures(1, &Texture);
         glBindTexture(GL_TEXTURE_2D, Texture);
         glTexImage2D(GL_TEXTURE_2D,
                      0,
                      GL_ALPHA,
                      GlobalState.FontData.Width,
                      GlobalState.FontData.Height,
                      0,
                      GL_ALPHA,
                      GL_UNSIGNED_BYTE,
                      GlobalState.FontData.Bitmap);
         #if 0
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
         #else
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
         #endif
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

         GlobalState.TextureHandle = Texture;
         stbi_write_png("out.png", GlobalState.FontData.Width, GlobalState.FontData.Height, 1, GlobalState.FontData.Bitmap, GlobalState.FontData.Width);
      }
      if(GlobalState.CameraView == 0.0f) {
         GlobalState.CameraView = 3.0f;
      } else {
#if 0
         f32 ZoomingSpeed = 0.01f;
         if(GlobalContext->Input->Up) {
            GlobalState.CameraView -= ZoomingSpeed;
         } else if(GlobalContext->Input->Down) {
            GlobalState.CameraView += ZoomingSpeed;
         }
#else
         if(GlobalContext->Input->Up) {
            TemporalY += 0.001f;
         }
         if(GlobalContext->Input->Down) {
            TemporalY -= 0.001f;
         }
         if(GlobalContext->Input->Right) {
            TemporalX += 0.001f;
         }
         if(GlobalContext->Input->Left) {
            TemporalX -= 0.001f;
         }
#endif
      }
      GlobalState.AspectRatioWidth  = 4;
      GlobalState.AspectRatioHeight = 3;
      s32 AppWidth  = GlobalContext->AppState->Width;
      s32 AppHeight = GlobalContext->AppState->Height;

      rect32 DisplayArea = FitDisplayAreaToAspectRatio(GlobalState.AspectRatioWidth, GlobalState.AspectRatioHeight,
                                                       GlobalContext->AppState->Width, GlobalContext->AppState->Height);
      glViewport(DisplayArea.MinX, DisplayArea.MinY, Rect32Width(DisplayArea), Rect32Height(DisplayArea));
      glClearColor(0.0f, 0.0f, 0.4f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);

      glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
      glBegin(GL_TRIANGLES);
      glColor3f(0.0f, 0.0f, 0.0f);

      glVertex2i(-1, 1);
      glVertex2i(1, 1);
      glVertex2i(-1, -1);

      glVertex2i(1, -1);
      glVertex2i(-1, -1);
      glVertex2i(1, 1);

      glEnd();

      frect32 Rect = {};
      frect32 TRect = {};

      Rect.X = 1.3f;
      Rect.Y = 0.1f;
      Rect.W = 1.0f;
      Rect.H = 1.0f;
      TRect.X = TemporalX;
      TRect.Y = TemporalY;
      TRect.W = Rect.W;
      TRect.H = Rect.H;
      f32 THH = TRect.H * 0.5f;
      f32 THW = TRect.W * 0.5f;
      f32 RHH = Rect.H * 0.5f;
      f32 RHW = Rect.W * 0.5f;
      b32 Intersect = (TRect.X + THW > Rect.X - RHW &&
                       TRect.X - THW < Rect.X + RHW &&
                       TRect.Y + THH > Rect.Y - THH &&
                       TRect.Y - THH < Rect.Y + THH);
      glColor3f(1.0f, 1.0f, 0.0f);
      DrawRectFromWorld(Rect);
      glColor3f(1.0f, 0.0f, Intersect ? 1.0f : 0.0f);
      DrawRectFromWorld(TRect);
      GlobalState.Console.Active = TRUE;
      if(GlobalState.Console.Active) {
         CARRAY_FOR_EACH(C, GlobalContext->Input->Characters) {
            u64 len = strlen(GlobalState.Console.Text);
            if((len + 1) < (ARRAY_LENGTH(GlobalState.Console.Text) - 1)) {
               GlobalState.Console.Text[len] = C;
            }
            else {
               memset(GlobalState.Console.Text, 0, sizeof(GlobalState.Console.Text));
            }
         }
         glColor3f(1.0f, 0.2f, 0.3f);
         DrawText(GlobalState.Console.Text, 0.0f, 0.0f, 10.0f);
         // if(GlobalContext.Pause)
      }

   }
}

void GameUnload(void) {
   glDeleteTextures(1, &GlobalState.TextureHandle);
}
