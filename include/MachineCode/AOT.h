#pragma once

#include "MachineCode/x86Code.h"

namespace db::aot
{

  void GenerateElf(const X86Code *code, const char *filePath);

}