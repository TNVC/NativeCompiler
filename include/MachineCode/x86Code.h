#pragma once

#include "ByteCode.h"
#include "Module/Module.h"

#include <cstddef>

namespace db
{

  #define AREA_SIZE(AREA) \
    ((uintptr_t) (AREA).end - (uintptr_t) (AREA).begin)

  const int64_t ENTRY0_ADDRESS = 0x400000;

  struct Area {
    std::byte *begin;
    std::byte *  end;
  };

  struct X86Code {
    size_t size;
    std::byte *data;
    struct {
      Area text;
      Area data;
      Area rodata;
    } area;
  };

  X86Code *GenerateX86CodeFromByteCode(ByteCode *byteCode);
  X86Code *GenerateX86CodeFromModule(Module *theModule);
  void DestroyX86Code(X86Code *code);

}