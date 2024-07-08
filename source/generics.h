#pragma once
#include <stdint.h>
#include <assert.h>
#include <string.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef  int8_t  s8;
typedef  int16_t s16;
typedef  int32_t s32;
typedef  int64_t s64;

typedef  int8_t  b8;
typedef  int16_t b16;
typedef  int32_t b32;
typedef  int64_t b64;

typedef float  f32;
typedef double f64;

#define TRUE  1
#define FALSE 0

#define ALWAYS_INLINE inline __attribute__((always_inline))
#define FORCE_INLINE inline __attribute__((__always_inline__, __gnu_inline__))
#define LIKELY(X)   (__builtin_expect(!!(X), 1))
#define UNLIKELY(X) (__builtin_expect(!!(X), 0))

#define PAGES_TO_BYTES(X) (X * 4096)
