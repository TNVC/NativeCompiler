#pragma once

#include <cstddef>

namespace db
{

  typedef int  data_t;
  typedef char  cmd_t;

  struct CmdHeader {
    unsigned char code  : 5;
    unsigned char mem   : 1;
    unsigned char reg   : 1;
    unsigned char immed : 1;
  };

  enum Register { rax, rbx, rcx, rdx, rex, rfx };

  enum OpCode {
    #define DEF_CMD(name, num, hasArg, isProducer, ...)  \
      OpCode_##name = num,

    #include "cmd.in"

    #undef DEF_CMD
  };

  struct Cmd {
    CmdHeader header;
    data_t data;
    cmd_t   reg;
  };

  struct ByteCode {
    size_t size;
    Cmd *data;
  };

  ByteCode *GetByteCode(const char *filePath);
  void DestroyByteCode(ByteCode *code);

}