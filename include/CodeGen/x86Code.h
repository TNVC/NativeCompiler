#pragma once

#include "Module/Module.h"
#include <cstddef>

namespace db {

  typedef unsigned char byte;

  struct Area {
    size_t size;
    size_t capacity;
    byte *data;
  };

  struct x86Code {
    Area text;
    Area rodata;
    Area data;
    struct {
      size_t offset;
      size_t size;
    } stdlib;
    size_t mainOffset;
    struct {
      size_t writingRodataOffset;
      size_t writingDataOffset;
    } references;
  };

  x86Code *GenerateX86Code(const Module *theModule);
  void DestroyX86Code(x86Code *code);
  bool GenerateELF(const x86Code *code, const char *filePath);

}