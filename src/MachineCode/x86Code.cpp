#include "MachineCode/x86Code.h"

#include <malloc.h>
#include <cstdio>
#include <cassert>

namespace db
{

  X86Code *GenerateX86Code(Module *theModule, CodeType type)
  {
    assert(theModule);

    X86Code *code =
        (X86Code *) calloc(1, sizeof(X86Code));
    if (!code)
      {
        fprintf(stderr,
                "Out of memory. File: \"%s\", Line: %d.\n",
                 __FILE__, __LINE__);
        return nullptr;
      }

    uint64_t startAddress =
        (type == CodeType::JIT) ?
        (uint64_t) code->data : ENTRY0_ADDRESS;

    return code;
  }

  void DestroyX86Code(X86Code *code)
  {
    assert(code);

    free(code->data);
    free(code);
  }

}