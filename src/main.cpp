#include "Module/Module.h"
#include "MachineCode/x86Code.h"
#include "MachineCode/JIT.h"
#include "MachineCode/AOT.h"

#include <cassert>

int main()
  {
    //db::ByteCode *byteCode =
    //    db::GetByteCode("../source/a.bin");
    //assert(byteCode);

    db::AST *ast =
        db::GetAST("../source/syntax.std");
    assert(ast);

    db::Module *theModule =
    //    db::GenerateModuleFromByteCode(byteCode);
        db::GenerateModuleFromAST(ast);
    assert(theModule);

    theModule->theModule->print(llvm::errs(), nullptr);

    db::X86Code *x86Code =
        db::GenerateX86CodeFromModule(theModule);
    assert(x86Code);

    //db::jit::ExecuteX86Code(x86Code);
    db::aot::GenerateElf(x86Code, "../source/a.out");

    db::DestroyX86Code(x86Code);
    db::DestroyModule(theModule);
    db::DestroyAST(ast);
    //db::DestroyByteCode(byteCode);
    return 0;
  }
