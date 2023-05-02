#pragma once

#include <cstddef>

namespace db
{

  enum ASTStatement {
    St   , If  , Else, Var ,
    While, Func, Ret , Call,
    Param, Eq  , Void, Type,
    Add  , Sub , Mul , Div ,
    Pow  , Cos , Sin , Tan ,
    Out  , In  , Endl, Sqrt,
    IsEE , IsNE, IsBT, IsGT,
    Mod  , And , Or  ,
    STATEMENT_COUNT
  };

  enum ASTNodeType { Statement, Name, Number, String };

  union ASTNodeValue {
    ASTStatement statement;
    char *name;
    double number;
    char *string;
  };

  struct ASTNode {
    ASTNodeType type;
    ASTNodeValue value;
    ASTNode * left;
    ASTNode *right;
  };

  struct AST {
    ASTNode *root;
  };

  AST *GetAST(const char *filePath);
  ASTNode *CreateStatement(ASTStatement statement);
  ASTNode *CreateString(const char *string);
  ASTNode *CreateName(const char *name);
  ASTNode *CreateNumber(double value);
  void DestroyAST(AST *ast);
  void DestroyASTNode(ASTNode *node);

}