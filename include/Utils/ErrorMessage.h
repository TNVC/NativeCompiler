#pragma once

#define OUT_OF_MEMORY(...)                              \
  do                                                    \
  {                                                     \
    fprintf(stderr,                                     \
            "Out of memory. File: \"%s\", Line: %d.\n", \
            __FILE__, __LINE__);                        \
    __VA_ARGS__;                                        \
  } while (false)

#define FAIL_TO_OPEN(FILE_NAME, ...)                          \
  do                                                          \
  {                                                           \
    fprintf(stderr,                                           \
            "Fail to open \"%s\". File: \"%s\", Line: %d.\n", \
            (FILE_NAME), __FILE__, __LINE__);                 \
    __VA_ARGS__;                                              \
  } while (false)