#include "MachineCode/JIT.h"

#include <cassert>

namespace db::jit
{

  typedef void (*function)();

  void ExecuteX86Code(const X86Code *code)
  {
    assert(code);

    ((function) code->text.data)();
  }

}
