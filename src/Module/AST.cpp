#include "Module/AST.h"

#include "Utils/ErrorMessage.h"

#include <malloc.h>
#include <cstring>
#include <cctype>
#include <cstdio>
#include <cassert>
#include <cmath>

#define ERROR(...)                                    \
  do {                                                \
    fprintf(stderr,                                   \
            "Broken file. File: \"%s\", Line: %d.\n", \
            __FILE__, __LINE__);                      \
    if (node) DestroyASTNode(node);                   \
    *hasError = true;                                 \
    return __VA_ARGS__;                               \
  } while (false)

namespace db
{

  const struct {
    const char *name;
    ASTStatement statement;
    size_t size;
  } STATEMENTS[] =
      {
        { "ST"   , St   , 2 }, { "IF"   , If   , 2 }, { "ELSE" , Else , 4 },
        { "VAR"  , Var  , 3 }, { "WHILE", While, 5 }, { "FUNC" , Func , 4 },
        { "RET"  , Ret  , 3 }, { "CALL" , Call , 4 }, { "PARAM", Param, 5 },
        { "EQ"   , Eq   , 2 }, { "VOID" , Void , 4 }, { "TYPE" , Type , 4 },
        { "ADD"  , Add  , 3 }, { "SUB"  , Sub  , 3 }, { "MUL"  , Mul  , 3 },
        { "DIV"  , Div  , 3 }, { "POW"  , Pow  , 3 }, { "COS"  , Cos  , 3 },
        { "SIN"  , Sin  , 3 }, { "TAN"  , Tan  , 3 }, { "OUT"  , Out  , 3 },
        { "IN"   , In   , 2 }, { "ENDL" , Endl , 4 }, { "SQRT" , Sqrt , 4 },
        { "IS_EE", IsEE , 5 }, { "IS_NE", IsNE , 5 }, { "IS_BT", IsBT , 5 },
        { "IS_GT", IsGT , 5 }, { "MOD"  , Mod  , 3 }, { "AND"  , And  , 3 },
        { "OR"   , Or   , 3 },
      };

  const size_t MAX_ID_SIZE = 8;
  const char ID[] = "db";
  const size_t MAX_SIZE = 128;

  static ASTNode *ReadASTNode(FILE *file, bool *hasError);

  static int stricmp(const char *first, const char *second);

  AST *GetAST(const char *filePath)
  {
    assert(filePath);

    FILE *file = fopen(filePath, "r");
    if (!file)
      {
        fprintf(stderr,
                "Fail to open \"%s\". File: \"%s\", Line: %d.\n",
                filePath, __FILE__, __LINE__);
        return nullptr;
      }

    AST *ast =
        (AST *) calloc(1, sizeof(AST));
    if (!ast)
      OUT_OF_MEMORY(return nullptr);

    bool hasError = false;
    ast->root =
        ReadASTNode(file, &hasError);

    return ast;
  }

  static ASTNode *ReadASTNode(FILE *file, bool *hasError)
  {
    assert(file && hasError);

    ASTNode *node = nullptr;

    char ch {'\0'};
    if (!fscanf(file, " %c", &ch)) return nullptr;

    bool isOptional = (ch == '$');
    if (isOptional)
      {
        char id[MAX_ID_SIZE] = "";
        fscanf(file, "%2s", id);
        if (!strcmp(id, ID)) fscanf(file, "%*[^ \t\n]");
        else
          {
            int depth = 1;
            while (depth)
              {
                fscanf(file, "%*[^$]$");
                ch = (char) getc(file);

                depth += isspace(ch) ? -1 : +1;
              }
            return nullptr;
          }

        if (!fscanf(file, " %c", &ch)) ERROR(nullptr);
      }

    if (ch != '{')
      { ungetc(ch, file); return nullptr; }

    char buffer[MAX_SIZE] = "";
    bool isString = false;

    fscanf(file, " %c", &ch);
    if (ch == '\"')
      {
        if (!fscanf(file, "%[^\"]\"", buffer)) ERROR(nullptr);
      }
    else if (ch == '\'')
      {
        isString = true;
        if (!fscanf(file, "%[^\']\'", buffer)) ERROR(nullptr);
      }
    else
      {
        ungetc(ch, file);
        if (!fscanf(file, " %s", buffer)) ERROR(nullptr);
      }

    if (!stricmp("NIL", buffer))
      {
        if (!fscanf(file, " %c", &ch) || ch != '}') ERROR(nullptr);
        return nullptr;
      }

    for (size_t i = 0; i < STATEMENT_COUNT; ++i)
      if (!stricmp(STATEMENTS[i].name, buffer))
      {
        node = CreateStatement(STATEMENTS[i].statement);
        if (!node) ERROR(nullptr);
        break;
      }

    if (!node)
      {
        double value = NAN;
        int offset = 0;
        sscanf(buffer, "%lg%n", &value, &offset);

        if      (buffer[offset] &&  isString)
          {
            node = CreateString(buffer);
            if (!node) ERROR(nullptr);
          }
        else if (buffer[offset] && !isString)
          {
            node = CreateName  (buffer);
            if (!node) ERROR(nullptr);
          }
        else
          {
            node = CreateNumber(value);
            if (!node) ERROR(nullptr);
          }
      }

    node->left  = ReadASTNode(file, hasError);
    if (*hasError) ERROR(nullptr);
    node->right = ReadASTNode(file, hasError);
    if (*hasError) ERROR(nullptr);

    if (!fscanf(file, " %c", &ch) || ch != '}')
      ERROR(nullptr);

    if (isOptional &&
    (!fscanf(file, " %c", &ch) || ch != '$'))
      ERROR(nullptr);

    return node;
  }

  ASTNode *CreateStatement(ASTStatement statement)
  {
    ASTNode *node =
        (ASTNode *) calloc(1, sizeof(ASTNode));
    if (!node)
      OUT_OF_MEMORY(return nullptr);

    node->type = Statement;
    node->value.statement = statement;

    return node;
  }
  ASTNode *CreateString(const char *string)
  {
    assert(string);

    ASTNode *node =
        (ASTNode *) calloc(1, sizeof(ASTNode));
    if (!node)
      OUT_OF_MEMORY(return nullptr);

    node->type = String;
    node->value.string = strdup(string);
    if (!node->value.string)
      OUT_OF_MEMORY(return nullptr);

    return node;
  }
  ASTNode *CreateName(const char *name)
  {
    assert(name);

    ASTNode *node =
        (ASTNode *) calloc(1, sizeof(ASTNode));
    if (!node)
      OUT_OF_MEMORY(return nullptr);

    node->type = Name;
    node->value.name = strdup(name);
    if (!node->value.name)
      OUT_OF_MEMORY(return nullptr);

    return node;
  }
  ASTNode *CreateNumber(double value)
  {
    ASTNode *node =
        (ASTNode *) calloc(1, sizeof(ASTNode));
    if (!node)
      OUT_OF_MEMORY(return nullptr);

    node->type = Number;
    node->value.number = value;

    return node;
  }

  void DestroyAST(AST *ast)
  {
    assert(ast);

    DestroyASTNode(ast->root);
    free(ast);
  }
  void DestroyASTNode(ASTNode *node)
  {
    assert(node);

    if (node->left)
      DestroyASTNode(node->left);
    if (node->right)
      DestroyASTNode(node->right);

    switch (node->type)
    {
      case Name: case String:
        { free(node->value.string); }
      default: break;
    }
    free(node);
  }

  static int stricmp(const char *first, const char *second)
  {
    assert(first);
    assert(second);

    int i = 0;

    for ( ; first[i] && second[i]; ++i)
      if (tolower(first[i]) != tolower(second[i]))
        return tolower(first[i]) - tolower(second[i]);

    return tolower(first[i]) - tolower(second[i]);
  }

}