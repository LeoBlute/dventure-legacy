#include "dventure.h"
#include <stdio.h>
#include <gl.h>

typedef struct game_state {
   u32 AspectRatioHeight;
   u32 AspectRatioWidth;
   u32 Width;
   u32 Height;
   f32 CameraView;
   file_data ChangedTTFData;
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
   glBegin(GL_TRIANGLE_STRIP);	
   glVertex2f( X - HW, Y - HH);
   glVertex2f( X - HW, Y + HH);
   glVertex2f( X + HW, Y - HH);
   glVertex2f( X + HW, Y + HH);
   glEnd();
}

static f32 TemporalX;
static f32 TemporalY;

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
      //*Unique changed id*/
      file_data ChangedTTFData = GlobalContext->Platform->FilesOfTypeData(".ttf");
      if(FILE_DATA_CHANGED(GlobalState.ChangedTTFData, ChangedTTFData)) {
         GlobalState.ChangedTTFData =  ChangedTTFData;
         buffer FillToSequence = {};
         FillToSequence.Data = ArenaAlloc(GlobalContext->AssetArena, GlobalState.ChangedTTFData.ContentSize);
         FillToSequence.Size = GlobalState.ChangedTTFData.ContentSize;
         ArenaReset(GlobalContext->AssetArena);
         GlobalContext->Platform->FilesOfTypeContent(".ttf", &FillToSequence);
         printf("PGChanged, %ld\n", GlobalState.ChangedTTFData.ContentSize);
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
            TemporalY += 0.01f;
         }
         if(GlobalContext->Input->Down) {
            TemporalY -= 0.01f;
         }
         if(GlobalContext->Input->Right) {
            TemporalX += 0.01f;
         }
         if(GlobalContext->Input->Left) {
            TemporalX -= 0.01f;
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
      glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
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

      glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
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
   }
}
