#pragma once

#include "AST.h"
#include "Symbol/Symbol.h"

#include "ClangAPI.h"

namespace db
{

  struct Module {
      llvm::LLVMContext *context;
      llvm::Module    *theModule;
      llvm::IRBuilder<> *builder;

      SymbolTable symTable;
    };

  Module *GenerateModuleFromAST(AST *ast);
  void DestroyModule(Module *theModule);

}