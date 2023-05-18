#include "Symbol/Symbol.h"

#include "Utils/ErrorMessage.h"

#include <malloc.h>
#include <cstring>
#include <cstdio>
#include <cassert>

namespace db
{

  const size_t DEFAULT_FACTOR = 10;

  Variable *AddGlobalVariable(SymbolTable *symTable, const char *name, llvm::Value *value)
  {
    assert(symTable && name);

    auto *globals = &symTable->globals;
    if (globals->size == globals->capacity)
      {
        size_t elementCount = 2*globals->size + DEFAULT_FACTOR;
        Variable *temp = (Variable *)
            reallocarray(globals->data, elementCount, sizeof(Variable));
        if (!temp)
          OUT_OF_MEMORY(return nullptr);

        globals->data = temp;
        globals->capacity = elementCount;
      }

    globals->data[globals->size] =
        { name, value };

    return globals->data + globals->size++;
  }

  Function *AddFunction(SymbolTable *symTable, const char *name)
  {
    assert(symTable && name);

    auto *functions = &symTable->functions;
    if (functions->size == functions->capacity)
      {
        size_t elementCount = 2*functions->size + DEFAULT_FACTOR;
        Function *temp = (Function *)
            reallocarray(functions->data, elementCount, sizeof(Function));
        if (!temp)
          OUT_OF_MEMORY(return nullptr);

        functions->data = temp;
        functions->capacity = elementCount;
      }

    functions->data[functions->size] =
        { name };

    return functions->data + functions->size++;
  }

  Variable *AddParam(Function *function, const char *name, llvm::Value *value)
  {
    assert(function && name);

    auto *params = &function->params;
    if (params->size == params->capacity)
      {
        size_t elementCount = 2*params->size + DEFAULT_FACTOR;
        Variable *temp = (Variable *)
            reallocarray(params->data, elementCount, sizeof(Variable));
        if (!temp)
          OUT_OF_MEMORY(return nullptr);

        params->data = temp;
        params->capacity = elementCount;
      }

    params->data[params->size] =
        { name, value };

    return params->data + params->size++;
  }

Variable *AddLocalVariable(Function *function, const char *name, llvm::Value *value)
  {
    assert(function && name);

    auto *locals = &function->locals;
    if (locals->size == locals->capacity)
      {
        size_t elementCount = 2*locals->size + DEFAULT_FACTOR;
        Variable *temp = (Variable *)
            reallocarray(locals->data, elementCount, sizeof(Variable));
        if (!temp)
          OUT_OF_MEMORY(return nullptr);

        locals->data = temp;
        locals->capacity = elementCount;
      }

    locals->data[locals->size] =
        { name, value };

    return locals->data + locals->size++;
  }

  Variable *GetGlobalOrNull(SymbolTable *symTable, const char *name)
  {
    assert(symTable && name);

    auto *globals = &symTable->globals;
    for (size_t i = 0; i < globals->size; ++i)
      if (!strcmp(globals->data[i].name, name))
        return &globals->data[i];
    return nullptr;
  }

  Variable *GetValueOrNull(Function *function, const char *name)
  {
    assert(function && name);

    auto *params = &function->params;
    for (size_t i = 0; i < params->size; ++i)
      if (!strcmp(params->data[i].name, name))
        return &params->data[i];
    auto *locals = &function->locals;
    for (size_t i = 0; i < locals->size; ++i)
      if (!strcmp(locals->data[i].name, name))
        return &locals->data[i];
    return nullptr;
  }

}