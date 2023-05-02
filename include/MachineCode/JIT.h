#pragma once

#include "MachineCode/x86Code.h"

namespace db::jit
{

  void ExecuteX86Code(const X86Code *code);

}
