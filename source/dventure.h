#pragma once

#include "generics.h"
#include "dmemory.h"
#include "dmath.h"
#include "containers.h"

typedef struct game_input {
   b8 Up;
   b8 Down;
   b8 Left;
   b8 Right;
   b8 Pause;
   CARRAY(char, 128) Characters;
   f32 Time;
   f32 DeltaTime;
} game_input;

typedef struct app_state {
   b32 Initialized;
   u32 Width;
   u32 Height;
} app_state;

typedef struct platform_procedures {
   void* (*Allocate) (u64);
   void  (*Deallocate) (void*, u64);
   void (*FilesOfTypeContent) (char*, buffer*);
   file_data (*FilesOfTypeData) (char*);
} platform_procedures;

typedef struct game_context {
   game_input* Input;
   app_state*  AppState;
   arena*      TransientArena;
   arena*      WorldArena;
   arena*      AssetArena;
   platform_procedures* Platform;
} game_context;

static void* TransientAlloc(u64 Size);

typedef void (*game_loop) (game_context*);
typedef void (*game_unload) (void);
