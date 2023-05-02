#include "MachineCode/JIT.h"

#include <cassert>

#include <sys/mman.h>

namespace db::jit
{

  typedef void (*function)();

  void ExecuteX86Code(const X86Code *code)
  {
    assert(code);

    //mprotect(code->data, code->size, PROT_EXEC | PROT_READ | PROT_WRITE);
    //((function)code->data)();

    mprotect(
        code->area.text.begin,
        AREA_SIZE(code->area.text),
        PROT_EXEC | PROT_READ | PROT_WRITE
        );
    mprotect(
        code->area.data.begin,
        AREA_SIZE(code->area.data),
        PROT_READ | PROT_WRITE
        );
    mprotect(
        code->area.rodata.begin,
        AREA_SIZE(code->area.rodata),
        PROT_READ
        );

    ((function)code->area.text.begin)();
  }

}
