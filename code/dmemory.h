#pragma once

#include "generics.h"

typedef struct buffer {
   u64   Size;
   char* Data;
} buffer;

typedef struct arena {
   u64 Size;
   char* Data;
   u64 AllocatedAmount;
   u64 Pad0;
} arena;

typedef struct string {
   u64   Size;
   char* Data;
} string;

//Unaligned Allocations
static void* ArenaAlloc(arena* Arena, u64 Size) {
   assert(Arena->AllocatedAmount + Size < Arena->Size);
   void* Memory = Arena->Data + Arena->AllocatedAmount;
   Arena->AllocatedAmount += Size;
   return Memory;
}

static void ResetArena(arena* Arena) {
   for(u64 i=0; i<Arena->Size; i++) {
      Arena->Data[i] = 0;
   }
   Arena->AllocatedAmount = 0;
}
