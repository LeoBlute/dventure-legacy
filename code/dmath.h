#pragma once
#include <stdlib.h>
#include "generics.h"

typedef struct rect32 {
   s32 MinX; s32 MinY;
   s32 MaxX; s32 MaxY;
} rect32;

typedef struct frect32 {
   f32 X; f32 Y;
   f32 W; f32 H;
} frect32;

FORCE_INLINE s32 Rect32Width(rect32 Rect) {
   return abs(Rect.MaxX - Rect.MinX);
}

FORCE_INLINE s32 Rect32Height(rect32 Rect) {
   return abs(Rect.MaxY - Rect.MinY);
}

/*FORCE_INLINE f32 FRect32Width(frect32 Rect) {
   return Rect.MaxX - Rect.MinX;
}

FORCE_INLINE f32 FRect32Height(frect32 Rect) {
   return Rect.MaxY - Rect.MinY;
}

FORCE_INLINE frect32 FRect32FromPosAndSize(f32 X, f32 Y, f32 W, f32 H) {
   frect32 Result;
   f32 HalfWidth  = W * 0.5f;
   f32 HalfHeight = H * 0.5f;
   Result.MinX = X - HalfWidth;
   Result.MaxX = X + HalfWidth;
   Result.MinY = Y - HalfHeight;
   Result.MaxY = Y + HalfHeight;
   assert(FRect32Width(Result) == W && FRect32Height(Result) == H);
   return Result;
}

FORCE_INLINE frec32 FRect32FromBox(fbox32 Box) {
   frect32 Result;

   return Result;
}*/
