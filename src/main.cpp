#include "Module/Module.h"
#include "MachineCode/x86Code.h"
#include "MachineCode/JIT.h"
#include "MachineCode/Native.h"

#include <cstdio>
#include <cstring>

int main(const int argc, const char *const argv[])
  {
    if (argc < 3) goto Exit;

    if (!strcmp(argv[1], "--jit"))
      {//../source/a.bin
        db::ByteCode *byteCode =
            db::GetByteCode(argv[2]);
        db::X86Code *x86Code = nullptr;
            //db::GenerateX86CodeFromByteCode(byteCode);

        db::jit::ExecuteX86Code(x86Code);

        db::DestroyX86Code(x86Code);
        db::DestroyByteCode(byteCode);
      }
    else
      {
        db::AST *ast =
            db::GetAST(argv[1]);
        db::Module *theModule =
            db::GenerateModuleFromAST(ast);

        theModule->theModule->print(llvm::errs(), nullptr);

        db::X86Code *x86Code =
            db::GenerateX86CodeFromModule(theModule);

        db::native::GenerateElf(x86Code, argv[2]);

        db::DestroyX86Code(x86Code);
        db::DestroyModule(theModule);
        db::DestroyAST(ast);
      }

    return 0;
  Exit:
    printf("No source file.\n"
           "Use %s [flags] [source file name] [destiny file name]\n"
           "Examples:\n"
           "%s --jit file.bin\n"
           "%s file.std file.out\n", argv[0], argv[0], argv[0]);
    return 0;
  }
