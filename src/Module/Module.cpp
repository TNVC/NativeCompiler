#include "Module/Module.h"

#include "Utils/ErrorMessage.h"

#include <cstdio>
#include <malloc.h>
#include <cassert>

namespace db
{

  const size_t BUFFER_SIZE = 24;
  static size_t NameIndex = 0;
  static void SetName(llvm::Value *value);

  const size_t MAX_NAME_SIZE = 64;
  static size_t BlockIndex = 0;
  char *GenerateName(const char *name);

  const llvm::Align DEFAULT_ALIGN(0x8);

  static void CreateLibrary(Module *theModule);

  /*Start: IR building function*/

  enum CmpType { EE, NE, BT, GT };
  enum LogicType { LAnd, LOr };
  enum CallType { SinCall, CosCall, TanCall, SqrtCall };

  llvm::Function *CreateFunction
    (Module *theModule, llvm::FunctionType *type,
      const char *name, std::vector<const char *> argsName);

  llvm::BasicBlock *CreateBasicBlock
    (Module *theModule, llvm::Function *function, const char *name);

  llvm::GlobalVariable *CreateGlobalVariable
    (Module *theModule, llvm::Constant *initValue, const char *name);

  llvm::Value *CreateLocalVariable
    (Module *theModule, llvm::Function *function, llvm::Value *initValue, const char *name);

  llvm::Constant  *CreateString
    (Module *theModule, const char *string);

  llvm::ReturnInst *CreateReturn
    (Module *theModule, llvm::Function *function, llvm::Value *retValue);

  llvm::Value *CreateAdd
    (Module *theModule, llvm::Value *first, llvm::Value *second);
  llvm::Value *CreateSub
    (Module *theModule, llvm::Value *first, llvm::Value *second);
  llvm::Value *CreateMul
    (Module *theModule, llvm::Value *first, llvm::Value *second);
  llvm::Value *CreateDiv
    (Module *theModule, llvm::Value *first, llvm::Value *second);
  llvm::Value *CreateCmp
    (Module *theModule, llvm::Value *first, llvm::Value *second, CmpType type);
  llvm::Value *CreateLogic
    (Module *theModule, llvm::Value *first, llvm::Value *second, LogicType type);
  llvm::Value *CreateMod
    (Module *theModule, llvm::Value *value);
  llvm::Value *CreateAssignment
    (Module *theModule, llvm::Value *first,  llvm::Value *second);
  llvm::Value *CreateNeg
    (Module *theModule, llvm::Value *value);

  llvm::Value *CreateLibraryCall
    (Module *theModule, llvm::Value *value, CallType type);
  llvm::Value *CreatePowCall
    (Module *theModule, llvm::Value *base, llvm::Value *power);

  llvm::Value *CreatePrintfCall
    (Module *theModule, llvm::ArrayRef<llvm::Value *> *values);
  llvm::Value *CreateScanfCall
    (Module *theModule, llvm::ArrayRef<llvm::Value *> *values);
  llvm::Value *CreateCall
    (Module *theModule, const char *name, llvm::ArrayRef<llvm::Value *> *values);

  llvm::Value *CreateIfStatement
    (Module *theModule, llvm::Value *value, std::vector<llvm::BasicBlock *> *blocks);
  llvm::Value *CreateWhileStatement
    (Module *theModule, llvm::Value *value, std::vector<llvm::BasicBlock *> *blocks);
  /*End: IR building function*/

  struct Status {
    bool inFunction;
    Function *functionSym;
    llvm::Function *function;
    llvm::Constant *endl;
  };

  static void CreateModule(Module *theModule, const char *name);

  static void *VisitASTNode(Module *theModule, Status *status, ASTNode *node);

  Module *GenerateModule(AST *ast)
    {
      assert(ast);

      Module *theModule =
          (Module *) calloc(1, sizeof(Module));
      if (!theModule)
        OUT_OF_MEMORY(return nullptr);

      CreateModule(theModule, "AST");

      CreateLibrary(theModule);
      llvm::Constant *endl =
          CreateString(theModule, "\n");

      Status status { .endl = endl };
      VisitASTNode(theModule, &status, ast->root);


      return theModule;
    }

  void DestroyModule(Module *theModule)
  {
    assert(theModule);

    free(theModule->symTable.globals.data);
    for (size_t i = 0; i < theModule->symTable.functions.size; ++i)
      free(theModule->symTable.functions.data[i].locals.data);
    free(theModule->symTable.functions.data);

    delete theModule->builder;
    delete theModule->theModule;
    delete theModule->context;

    free(theModule);
  }
  static void CreateModule(Module *theModule, const char *name)
  {
    assert(theModule && name);

    theModule->context   = new llvm::LLVMContext;
    theModule->theModule = new llvm::Module(name, *theModule->context);
    theModule->builder   = new llvm::IRBuilder(*theModule->context);

    theModule->symTable = {};
 }

 static void *VisitASTNode(Module *theModule, Status *status, ASTNode *node)
 {
   assert(theModule && status && node);

   switch (node->type)
   {
     case Name:
       {
         Variable *var =
             GetGlobalOrNull(&theModule->symTable, node->value.name);
         if (var) return var->value;
         var =
             GetValueOrNull(status->functionSym, node->value.name);
         return var->value;
       }
     case Number:
       {
         double number = node->value.number;
         llvm::Value *value =
             llvm::ConstantFP::get(*theModule->context, llvm::APFloat(number));
         return value;
       }
     case String:
       {
         char *string = node->value.string;
         llvm::Constant *stringRef =
             CreateString(theModule, string);
         return stringRef;
       }
     default: break;
   }

   switch (node->value.statement)
   {
     case If:
       {
        if (!status->inFunction) return nullptr;

        llvm::Value *cond =
            (llvm::Value *) VisitASTNode(theModule, status, node->left);

        bool hasElse = (node->right->value.statement == Else);

        llvm::BasicBlock *currentBlock =
            theModule->builder->GetInsertBlock();

        llvm::BasicBlock * thenBlock =
            CreateBasicBlock(theModule, status->function, GenerateName("then"));
        theModule->builder->SetInsertPoint(thenBlock);
        VisitASTNode(theModule, status, hasElse ? node->right->left : node->right);
        llvm::BasicBlock * firstBlock =
            theModule->builder->GetInsertBlock();

        llvm::BasicBlock * elseBlock = nullptr;
        llvm::BasicBlock *secondBlock = nullptr;
        if (hasElse)
          {
            elseBlock = CreateBasicBlock(theModule, status->function, GenerateName("else"));
            theModule->builder->SetInsertPoint(elseBlock);
            VisitASTNode(theModule, status, node->right->right);
            secondBlock =
                theModule->builder->GetInsertBlock();
          }
        llvm::BasicBlock *mergeBlock =
            CreateBasicBlock(theModule, status->function, GenerateName("merge"));
        theModule->builder->SetInsertPoint(mergeBlock);

        std::vector<llvm::BasicBlock *> blocks
          { thenBlock, elseBlock, firstBlock, secondBlock, mergeBlock, currentBlock };
        CreateIfStatement(theModule, cond, &blocks);
        return nullptr;
       }
     case Var:
     {
       char *name = node->left->value.name;

       void *value =
           node->right ?
           VisitASTNode(theModule, status, node->right) :
           nullptr;

       if (status->inFunction)
         {
           llvm::Value *var =
               CreateLocalVariable(theModule, status->function, (llvm::Value *) value, name);
           AddLocalVariable(status->functionSym, name, var);
         }
       else
         {
           llvm::Value *var =
               CreateGlobalVariable(theModule, (llvm::Constant *) value, name);
           AddGlobalVariable(&theModule->symTable, name, var);
         }

       return nullptr;
     }
     case While:
       {
         if (!status->inFunction) return nullptr;

         llvm::BasicBlock *currentBlock =
             theModule->builder->GetInsertBlock();

         llvm::BasicBlock *startBlock =
             CreateBasicBlock(theModule, status->function, GenerateName("start"));
         theModule->builder->SetInsertPoint(startBlock);
         VisitASTNode(theModule, status, node->right);
         llvm::Value *cond =
            (llvm::Value *) VisitASTNode(theModule, status, node->left);
         llvm::BasicBlock *  endBlock =
             CreateBasicBlock(theModule, status->function, GenerateName("end"));

         std::vector<llvm::BasicBlock *> blocks =
             { currentBlock, startBlock, endBlock };
         CreateWhileStatement(theModule, cond, &blocks);
         return nullptr;
       }
     case Func:
      {
        NameIndex  = 0;
        BlockIndex = 0;
        status->inFunction = true;

        const char *name = node->left->value.name;
        status->functionSym =
            AddFunction(&theModule->symTable, name);

        std::vector<const char *> paramsNames{};

        ASTNode *param = node->left->left;
        for ( ; param; param = param->right)
          {
            const char *paramName =
                param->left->left->value.name;
            paramsNames.push_back(paramName);
          }

        llvm::Type *type =
            llvm::Type::getDoubleTy(*theModule->context);
        llvm::Type *retType =
            node->left->right->value.statement == Void ?
            llvm::Type::getVoidTy(*theModule->context) : type;
        std::vector<llvm::Type *> params(paramsNames.size(), type);

        llvm::FunctionType *functionType =
            llvm::FunctionType::get(retType, params, false);

        llvm::Function *function =
            CreateFunction(theModule, functionType, name, paramsNames);

        llvm::BasicBlock *entry =
            CreateBasicBlock(theModule, function, GenerateName("entry"));
        theModule->builder->SetInsertPoint(entry);

        param = node->left->left;
        for (size_t i = 0; param; param = param->right, ++i)
          {
            const char *paramName =
                param->left->left->value.name;
            llvm::Value *value =
                function->getArg(i);
            AddParam(status->functionSym, paramName, value);
          }

        status->function = function;
        VisitASTNode(theModule, status, node->right);
        status->inFunction = false;

        if (retType->isVoidTy())
          CreateReturn(theModule, function, nullptr);

        return nullptr;
      }
     case Ret:
       {
         if (!status->inFunction) return nullptr;

         llvm::Value *retValue = nullptr;
         if (node->left)
           retValue =
               (llvm::Value *) VisitASTNode(theModule, status, node->left);

         CreateReturn(theModule, status->function, retValue);
         return nullptr;
       }
     case Call:
     {
       if (!status->inFunction) return nullptr;

       const char *name = node->left->value.name;

       std::vector<llvm::Value *> params{};

       ASTNode *temp = node->left->left;
       for ( ; temp; temp = temp->right)
         params.push_back((llvm::Value *) VisitASTNode(theModule, status, temp));

       llvm::ArrayRef<llvm::Value *> llvmParams(params);

       return CreateCall(theModule, name, &llvmParams);
     }
     case Eq:
       {
         if (!status->inFunction) return nullptr;

         llvm::Value * firstOperand =
             (llvm::Value *) VisitASTNode(theModule, status, node->left );
         llvm::Value *secondOperand =
             (llvm::Value *) VisitASTNode(theModule, status, node->right);

         llvm::Value *result =
             CreateAssignment(theModule, firstOperand, secondOperand);

         return result;
       }
     case Add:
       {
         if (!status->inFunction) return nullptr;

         llvm::Value * firstOperand =
             (llvm::Value *) VisitASTNode(theModule, status, node->left );
         llvm::Value *secondOperand =
             (llvm::Value *) VisitASTNode(theModule, status, node->right);

         llvm::Value *result =
             CreateAdd(theModule, firstOperand, secondOperand);

         return result;
       }
     case Sub:
       {
         if (!status->inFunction) return nullptr;

         bool isBinary = node->right;

         llvm::Value * firstOperand =
             (llvm::Value *) VisitASTNode(theModule, status, node->left );

         llvm::Value *secondOperand = nullptr;
         if (isBinary)
           secondOperand =
             (llvm::Value *) VisitASTNode(theModule, status, node->right);

         llvm::Value *result = isBinary ?
             CreateSub(theModule, firstOperand, secondOperand) :
             CreateNeg(theModule, firstOperand);

         return result;
       }
     case Mul:
       {
         if (!status->inFunction) return nullptr;

         llvm::Value * firstOperand =
             (llvm::Value *) VisitASTNode(theModule, status, node->left );
         llvm::Value *secondOperand =
             (llvm::Value *) VisitASTNode(theModule, status, node->right);

         llvm::Value *result =
             CreateMul(theModule, firstOperand, secondOperand);

         return result;
       }
     case Div:
       {
         if (!status->inFunction) return nullptr;

         llvm::Value * firstOperand =
             (llvm::Value *) VisitASTNode(theModule, status, node->left );
         llvm::Value *secondOperand =
             (llvm::Value *) VisitASTNode(theModule, status, node->right);

         llvm::Value *result =
             CreateDiv(theModule, firstOperand, secondOperand);

         return result;
       }
     case Pow:
       {
         if (!status->inFunction) return nullptr;

         llvm::Value *  base =
             (llvm::Value *) VisitASTNode(theModule, status, node->left );
         llvm::Value *power =
             (llvm::Value *) VisitASTNode(theModule, status, node->right);

         llvm::Value *result =
             CreatePowCall(theModule, base, power);

         return result;
       }
     case Cos:
       {
         if (!status->inFunction) return nullptr;

         llvm::Value *value =
             (llvm::Value *) VisitASTNode(theModule, status, node->left);

         llvm::Value *result =
             CreateLibraryCall(theModule, value, CosCall);

         return result;
       }
     case Sin:
       {
         if (!status->inFunction) return nullptr;

         llvm::Value *value =
             (llvm::Value *) VisitASTNode(theModule, status, node->left);

         llvm::Value *result =
             CreateLibraryCall(theModule, value, SinCall);

         return result;
       }
     case Tan:
       {
         if (!status->inFunction) return nullptr;

         llvm::Value *value =
             (llvm::Value *) VisitASTNode(theModule, status, node->left);

         llvm::Value *result =
             CreateLibraryCall(theModule, value, TanCall);

         return result;
       }
     case Sqrt:
       {
         if (!status->inFunction) return nullptr;

         llvm::Value *value =
             (llvm::Value *) VisitASTNode(theModule, status, node->left);

         llvm::Value *result =
             CreateLibraryCall(theModule, value, SqrtCall);

         return result;
       }
     case Out:
     {
       if (!status->inFunction) return nullptr;

       std::vector<llvm::Value *> params{};

       ASTNode *temp = node->left;
       for ( ; temp; temp = temp->right)
         params.push_back((llvm::Value *) VisitASTNode(theModule, status, temp->left));

       llvm::ArrayRef<llvm::Value *> llvmParams(params);
       CreatePrintfCall(theModule, &llvmParams);
       return nullptr;
     }
     case In:
     {
       if (!status->inFunction) return nullptr;

       std::vector<llvm::Value *> params{};

       ASTNode *temp = node->left;
       for ( ; temp; temp = temp->right)
         params.push_back((llvm::Value *) VisitASTNode(theModule, status, node->left));

       llvm::ArrayRef<llvm::Value *> llvmParams(params);
       CreateScanfCall(theModule, &llvmParams);
       return nullptr;
     }
     case Endl: return status->endl;
     case IsEE:
       {
         if (!status->inFunction) return nullptr;

         llvm::Value * firstOperand =
             (llvm::Value *) VisitASTNode(theModule, status, node->left );
         llvm::Value *secondOperand =
             (llvm::Value *) VisitASTNode(theModule, status, node->right);

         llvm::Value *result =
             CreateCmp(theModule, firstOperand, secondOperand, EE);

         return result;
       }
     case IsNE:
       {
         if (!status->inFunction) return nullptr;

         llvm::Value * firstOperand =
             (llvm::Value *) VisitASTNode(theModule, status, node->left );
         llvm::Value *secondOperand =
             (llvm::Value *) VisitASTNode(theModule, status, node->right);

         llvm::Value *result =
             CreateCmp(theModule, firstOperand, secondOperand, NE);

         return result;
       }
     case IsBT:
       {
         if (!status->inFunction) return nullptr;

         llvm::Value * firstOperand =
             (llvm::Value *) VisitASTNode(theModule, status, node->left );
         llvm::Value *secondOperand =
             (llvm::Value *) VisitASTNode(theModule, status, node->right);

         llvm::Value *result =
             CreateCmp(theModule, firstOperand, secondOperand, BT);

         return result;
       }
     case IsGT:
       {
         if (!status->inFunction) return nullptr;

         llvm::Value * firstOperand =
             (llvm::Value *) VisitASTNode(theModule, status, node->left );
         llvm::Value *secondOperand =
             (llvm::Value *) VisitASTNode(theModule, status, node->right);

         llvm::Value *result =
             CreateCmp(theModule, firstOperand, secondOperand, GT);

         return result;
       }
     case Mod: return nullptr;
       {
         if (!status->inFunction) return nullptr;

         llvm::Value * value =
             (llvm::Value *) VisitASTNode(theModule, status, node->left );

         llvm::Value *result =
             CreateMod(theModule, value);

         return result;
       }
     case And:
       {
         if (!status->inFunction) return nullptr;

         llvm::Value * firstOperand =
             (llvm::Value *) VisitASTNode(theModule, status, node->left );
         llvm::Value *secondOperand =
             (llvm::Value *) VisitASTNode(theModule, status, node->right);

         llvm::Value *result =
             CreateLogic(theModule, firstOperand, secondOperand, LAnd);

         return result;
       }
     case Or:
       {
         if (!status->inFunction) return nullptr;

         llvm::Value * firstOperand =
             (llvm::Value *) VisitASTNode(theModule, status, node->left );
         llvm::Value *secondOperand =
             (llvm::Value *) VisitASTNode(theModule, status, node->right);

         llvm::Value *result =
             CreateLogic(theModule, firstOperand, secondOperand, LOr);

         return result;
       }
     case Param:
       return VisitASTNode(theModule, status, node->left);
     default: break;
   }

   if (node->left)
       VisitASTNode(theModule, status, node->left);
   if (node->right)
     VisitASTNode(theModule, status, node->right);
   return nullptr;
 }

 static void CreateLibrary(Module *theModule)
 {
   assert(theModule);

   llvm::Type *type =
       theModule->builder->getDoubleTy();
   llvm::FunctionType *functionType =
       llvm::FunctionType::get(type, { type },  false);
   CreateFunction(theModule, functionType, "sin" , { "value" });
   CreateFunction(theModule, functionType, "cos" , { "value" });
   CreateFunction(theModule, functionType, "tan" , { "value" });
   CreateFunction(theModule, functionType, "sqrt", { "value" });
   functionType =
       llvm::FunctionType::get(type, { type, type }, false);
   CreateFunction(theModule, functionType, "pow", { "base", "power" });

   type = theModule->builder->getDoubleTy();
   llvm::Type *voidType =
       theModule->builder->getVoidTy();
   functionType =
       llvm::FunctionType::get(voidType, { type }, false);
   CreateFunction(theModule, functionType, "printDouble" , { "name" });

   functionType =
       llvm::FunctionType::get(type, false);
   CreateFunction(theModule, functionType,  "scanDouble" , {});

   type = theModule->builder->getInt8PtrTy();
   functionType =
        llvm::FunctionType::get(voidType, { type }, false);
   CreateFunction(theModule, functionType, "printString" , { "name" });
 }

  llvm::Function *CreateFunction
    (Module *theModule, llvm::FunctionType *type,
       const char *name, std::vector<const char *> argsName)
  {
    assert(theModule && type && name);

    llvm::Function *function =
        llvm::Function::Create(type, llvm::Function::ExternalLinkage, name, theModule->theModule);

    llvm::Function::arg_iterator
      iter = function->arg_begin(),
      end  = function->arg_end  ();
    for (size_t i = 0; iter != end; ++iter, ++i)
      iter->setName(argsName[i]);

    return function;
  }

  llvm::BasicBlock *CreateBasicBlock
    (Module *theModule, llvm::Function *function, const char *name)
    {
      assert(theModule && function && name);

      return llvm::BasicBlock::Create(*theModule->context, name, function);
    }

  llvm::GlobalVariable *CreateGlobalVariable
    (Module *theModule, llvm::Constant *initValue, const char *name)
     {
       assert(theModule && name);

       llvm::Type *type =
           llvm::Type::getDoubleTy(*theModule->context);
       theModule->theModule->getOrInsertGlobal(name, type);
       llvm::GlobalVariable *globalVariable =
           theModule->theModule->getNamedGlobal(name);
       globalVariable->setLinkage(llvm::GlobalValue::CommonLinkage);
       globalVariable->setAlignment(DEFAULT_ALIGN);
       if (initValue)
         globalVariable->setInitializer(initValue);

       return globalVariable;
     }

  llvm::Value *CreateLocalVariable
    (Module *theModule, llvm::Function *function, llvm::Value *initValue, const char *name)
    {
      assert(theModule && function && name);

      llvm::Type *type =
          llvm::Type::getDoubleTy(*theModule->context);

      if (initValue)
        {
          llvm::Value *value =
              theModule->builder->CreateLoad(type, initValue, name);
          return value;
        }
      else
        {
          llvm::Value *value =
              theModule->builder->CreateAlloca(type, nullptr, name);
          SetName(value);
          return value;
        }
    }

  llvm::Constant *CreateString
    (Module *theModule, const char *string)
    {
      assert(theModule && string);

      static const size_t MAX_NAME_SIZE = 16;
      static size_t index = 0;
      static char name[MAX_NAME_SIZE] = "";
      sprintf(name, "GlobalStr%zu", index++);

      llvm::Constant *constant =
          theModule->builder->CreateGlobalStringPtr(llvm::StringRef(string), name, 0, theModule->theModule);
      return constant;
    }

  llvm::ReturnInst *CreateReturn
    (Module *theModule, llvm::Function *function, llvm::Value *retValue)
    {
      assert(theModule && function);

      llvm::ReturnInst *returnInst =
          theModule->builder->CreateRet(retValue);
      return returnInst;
    }

  llvm::Value *CreateAdd
      (Module *theModule, llvm::Value *first, llvm::Value *second)
    {
      assert(theModule && first && second);

      llvm::Value *value =
          theModule->builder->CreateFAdd(first, second);
      SetName(value);
      return value;
    }

  llvm::Value *CreateSub
      (Module *theModule, llvm::Value *first, llvm::Value *second)
    {
      assert(theModule && first && second);

      llvm::Value *value =
          theModule->builder->CreateFSub(first, second);
      SetName(value);
      return  value;
    }

  llvm::Value *CreateMul
      (Module *theModule, llvm::Value *first, llvm::Value *second)
    {
      assert(theModule && first && second);

      llvm::Value *value =
          theModule->builder->CreateFMul(first, second);
      SetName(value);
      return value;
    }

  llvm::Value *CreateDiv
      (Module *theModule, llvm::Value *first, llvm::Value *second)
    {
      assert(theModule && first && second);

      llvm::Value *value =
          theModule->builder->CreateFDiv(first, second);
      SetName(value);
      return value;
    }

  llvm::Value *CreateCmp
    (Module *theModule, llvm::Value *first, llvm::Value *second, CmpType type)
    {
      assert(theModule && first && second);

      llvm::CmpInst::Predicate predicate {};
      switch (type)
      {
        case EE: { predicate = llvm::CmpInst::FCMP_OEQ; break; }
        case NE: { predicate = llvm::CmpInst::FCMP_ONE; break; }
        case BT: { predicate = llvm::CmpInst::FCMP_OLT; break; }
        case GT: { predicate = llvm::CmpInst::FCMP_OGT; break; }
      }

      llvm::Value *value =
          theModule->builder->CreateFCmp(predicate, first, second);
      SetName(value);
      return value;
    }

  llvm::Value *CreateLogic
    (Module *theModule, llvm::Value *first, llvm::Value *second, LogicType type)
    {
      assert(theModule && first && second);

      switch (type)
      {
        case LAnd:
          {
            llvm::Value *value =
                theModule->builder->CreateLogicalAnd(first, second);
            SetName(value);
            return value;
          }
        case LOr:
          {
            llvm::Value *value =
                theModule->builder->CreateLogicalOr (first, second);
            SetName(value);
            return value;
          }
      }
    }

  llvm::Value *CreateMod
    (Module *theModule, llvm::Value *value)
    {
      assert(theModule && value);

      llvm::Type *intType =
          theModule->builder->getInt64Ty();
      llvm::Type * fpType =
          theModule->builder->getDoubleTy();

      llvm::Value *temp0 =
          theModule->builder->CreateFPToSI(value, intType);
      SetName(temp0);
      llvm::Value *temp1 =
          theModule->builder->CreateSIToFP(temp0,  fpType);
      SetName(temp1);
      return temp1;
    }

  llvm::Value *CreateLibraryCall
    (Module *theModule, llvm::Value *value, CallType type)
    {
      assert(theModule && value);

      const char *name = nullptr;
      switch (type)
      {
        case  SinCall: { name = "sin" ; break; }
        case  CosCall: { name = "cos" ; break; }
        case  TanCall: { name = "tan" ; break; }
        case SqrtCall: { name = "sqrt"; break; }
      }

      llvm::Function *function =
          theModule->theModule->getFunction(name);

      llvm::Value *temp =
          theModule->builder->CreateCall(function, { value });
      SetName(temp);
      return temp;
    }

  llvm::Value *CreatePowCall
    (Module *theModule, llvm::Value *base, llvm::Value *power)
    {
      assert(theModule && base && power);

      llvm::Function *function =
          theModule->theModule->getFunction("pow");

      llvm::Value *value =
          theModule->builder->CreateCall(function, { base, power });
      SetName(value);
      return value;
    }

  llvm::Value *CreatePrintfCall
    (Module *theModule, llvm::ArrayRef<llvm::Value *> *values)
    {
      assert(theModule && values);

      llvm::Function *printString =
          theModule->theModule->getFunction("printString");
      llvm::Function *printDouble =
          theModule->theModule->getFunction("printDouble");

      size_t size = values->size();
      for (size_t i = 0; i < size; ++i)
        if (values->data()[i]->getType()->isDoubleTy())
          theModule->builder->CreateCall(printDouble, values->data()[i]);
        else
          theModule->builder->CreateCall(printString, values->data()[i]);
      return nullptr;
    }

  llvm::Value *CreateScanfCall
    (Module *theModule, llvm::ArrayRef<llvm::Value *> *values)
    {
      assert(theModule && values);

      llvm::Function *function =
          theModule->theModule->getFunction("scanDouble");

      llvm::Type *type = theModule->builder->getDoubleTy();
      size_t size = values->size();
      for (size_t i = 0; i < size; ++i)
        {
          llvm::Value *value =
              theModule->builder->CreateCall(function);
          SetName(value);
          theModule->builder->CreateStore(values->data()[i], value);
        }

      return nullptr;
    }

  llvm::Value *CreateCall
    (Module *theModule, const char *name, llvm::ArrayRef<llvm::Value *> *values)
    {
      assert(theModule && name && values);

      llvm::Function *function =
          theModule->theModule->getFunction(name);

      llvm::Value *value =
          theModule->builder->CreateCall(function, *values);
      SetName(value);
      return value;
    }

  llvm::Value *CreateAssignment
    (Module *theModule, llvm::Value *first,  llvm::Value *second)
    {
      assert(theModule && first && second);

      return theModule->builder->CreateStore(first, second);
    }

  llvm::Value *CreateIfStatement
    (Module *theModule, llvm::Value *value, std::vector<llvm::BasicBlock *> *blocks)
    {
      assert(theModule && value && blocks);

      theModule->builder->SetInsertPoint(blocks->at(5));

      if (blocks->at(1))
        theModule->builder->CreateCondBr(value, blocks->at(0), blocks->at(1));
      else
        theModule->builder->CreateCondBr(value, blocks->at(0), blocks->at(4));

      theModule->builder->SetInsertPoint(blocks->at(2));
      theModule->builder->CreateBr(blocks->at(4));

      if (blocks->at(1))
        {
          theModule->builder->SetInsertPoint(blocks->at(3));
          theModule->builder->CreateBr(blocks->at(4));
        }

      theModule->builder->SetInsertPoint(blocks->at(4));

      return nullptr;
    }

  llvm::Value *CreateNeg
    (Module *theModule, llvm::Value *value)
    {
      assert(theModule && value);

      llvm::Value *zero =
          llvm::ConstantFP::get(*theModule->context, llvm::APFloat(.0));

      llvm::Value *temp =
          theModule->builder->CreateFSub(zero, value);
      SetName(temp);
      return temp;
    }

  llvm::Value *CreateWhileStatement
    (Module *theModule, llvm::Value *value, std::vector<llvm::BasicBlock *> *blocks)
    {
      assert(theModule && value && blocks);

      theModule->builder->CreateCondBr(value, blocks->at(1), blocks->at(2));

      theModule->builder->SetInsertPoint(blocks->at(0));
      theModule->builder->CreateBr(blocks->at(1));

      theModule->builder->SetInsertPoint(blocks->at(2));

      return nullptr;
    }

    static void SetName(llvm::Value *value)
      {
        assert(value);
        char buffer[BUFFER_SIZE] = "";
        sprintf(buffer, "%zu", NameIndex++);
        value->setName(buffer);
      }

    char *GenerateName(const char *name)
      {
        assert(name);
        static char buffer[MAX_NAME_SIZE]{};
        sprintf(buffer, "%s%zu", name, BlockIndex++);
        return buffer;
      }
}