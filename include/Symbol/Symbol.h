#pragma once

#include "ClangAPI.h"
#include <cstddef>

namespace db
{

  struct Variable {
    const char *name;
    llvm::Value *value;
  };

  struct Function {
    const char *name;
    struct {
      size_t size;
      size_t capacity;
      Variable *data;
    } params;
    struct {
      size_t size;
      size_t capacity;
      Variable *data;
    } locals;
  };

  struct SymbolTable {
    struct {
      size_t size;
      size_t capacity;
      Function *data;
    } functions;
    struct {
      size_t size;
      size_t capacity;
      Variable *data;
    } globals;
  };

  Variable *AddGlobalVariable
    (SymbolTable *symTable, const char *name, llvm::Value *value);
  Function *AddFunction(SymbolTable *symTable, const char *name);
  Variable *AddParam
    (Function *function, const char *name, llvm::Value *value);
  Variable *AddLocalVariable
    (Function *function, const char *name, llvm::Value *value);

  Variable *GetGlobalOrNull(SymbolTable *symTable, const char *name);
  Variable *GetValueOrNull(Function *function, const char *name);

}