#pragma once

#define OUT_OF_MEMORY(...)                              \
  do                                                    \
  {                                                     \
    fprintf(stderr,                                     \
            "Out of memory. File: \"%s\", Line: %d.\n", \
            __FILE__, __LINE__);                        \
    __VA_ARGS__;                                        \
  } while (false)

