#pragma once

#include "ByteCode.h"
#include "Module/Module.h"

#include <cstddef>

namespace db
{

  const int64_t ENTRY0_ADDRESS = 0x400000;

  struct Flashing {
    size_t rodata;
    size_t   data;
    size_t mainAddress;
  };

  struct Area {
    size_t size;
    size_t capacity;
    std::byte *data;
  };

  struct X86Code {
    bool isJIT;
    Area text;
    Area data;
    Area rodata;
    Flashing flashing;
  };

  X86Code *GenerateX86CodeFromByteCode(ByteCode * byteCode);
  X86Code *GenerateX86CodeFromModule  (Module   *theModule);
  void Write(X86Code *code, const void *buffer, size_t size);
  void DestroyX86Code(X86Code *code);

}