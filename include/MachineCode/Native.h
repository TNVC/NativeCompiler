#pragma once

#include "MachineCode/x86Code.h"

namespace db::native
{

  void GenerateElf(const X86Code *code, const char *filePath);

}