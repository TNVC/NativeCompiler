#pragma once

#include <cstdarg>
#include <malloc.h>
#include <cstddef>

#define FreeAll(...) FreeAll_(VA_ARGS_SIZE(__VA_ARGS__) __VA_OPT__(,) __VA_ARGS__)

#define VA_ARGS_SIZE(...) \
    VA_ARGS_COUNTER(__VA_ARGS__ __VA_OPT__(,) 16,15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define VA_ARGS_COUNTER(_0 , _1 , _2 , _3 , _4 , _5 , _6 , _7 , \
                        _8 , _9 , _10, _11, _12, _13, _14, _15, N, ...) N

inline void FreeAll_(size_t size, ...)
  {
    va_list args{};
    va_start(args, size);
    for (size_t i = 0; i < size; ++i) free(va_arg(args, void *));
    va_end(args);
  }