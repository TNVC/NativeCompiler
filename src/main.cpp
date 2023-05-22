#include "Module/Module.h"
#include "CodeGen/x86Code.h"

#include <cstdio>

int main(const int argc, const char *const argv[])
  {
    if (argc < 3)
      {
        printf("No source file.\n"
               "Use %s [source file name] [destiny file name]\n" ,
               argv[0]);
        return 0;
      }

    db::AST *ast =
        db::GetAST(argv[1]);
    db::Module *theModule =
        db::GenerateModule(ast);

    theModule->theModule->print(llvm::errs(), nullptr);

    db::x86Code *code =
        db::GenerateX86Code(theModule);
    db::GenerateELF(code, argv[2]);

    db::DestroyX86Code(code);
    db::DestroyModule(theModule);
    db::DestroyAST(ast);
    return 0;
  }
