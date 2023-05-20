#include "MachineCode/x86Code.h"

#include "Utils/ErrorMessage.h"
#include "MachineCode/x86cmd.in"

#include <malloc.h>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <cctype>

namespace db
{

  struct SecondByte {
    unsigned char mapSelect : 5;
    unsigned char B         : 1;
    unsigned char X         : 1;
    unsigned char R         : 1;
  };
  struct ThirdByte {
    unsigned char pp   : 2;
    unsigned char L    : 1;
    unsigned char vvvv : 4;
    unsigned char W    : 1;
  };
  struct FifthByte {
    unsigned char second : 3;
    unsigned char  first : 3;
    unsigned char mode   : 2;
  };

  const size_t CMD_SIZE = 5;
  const unsigned char VEX_PREFIX = 0xC4;
  const size_t REG_COUNT = 16;

  #include "CodeGen.cpp.in"

  #define GenerateFAddInstruction(CONTEXT, CODE, INST, INDEX) GenerateBinaryOperator(CONTEXT, CODE, INST, FAdd, INDEX)
  #define GenerateFSubInstruction(CONTEXT, CODE, INST, INDEX) GenerateBinaryOperator(CONTEXT, CODE, INST, FSub, INDEX)
  #define GenerateFMulInstruction(CONTEXT, CODE, INST, INDEX) GenerateBinaryOperator(CONTEXT, CODE, INST, FMul, INDEX)
  #define GenerateFDivInstruction(CONTEXT, CODE, INST, INDEX) GenerateBinaryOperator(CONTEXT, CODE, INST, FDiv, INDEX)
  #define  GenerateAndInstruction(CONTEXT, CODE, INST, INDEX) GenerateBinaryOperator(CONTEXT, CODE, INST, LAnd, INDEX)
  #define   GenerateOrInstruction(CONTEXT, CODE, INST, INDEX) GenerateBinaryOperator(CONTEXT, CODE, INST, LOr , INDEX)

  #define  GenerateLoadInstruction(CONTEXT, CODE, INST, INDEX) GenerateAssignmentOperator(CONTEXT, CODE, INST, INDEX)
  #define GenerateStoreInstruction(CONTEXT, CODE, INST, INDEX) GenerateAssignmentOperator(CONTEXT, CODE, INST, INDEX)

  enum InstructionType { FAdd, FMul, FSub, FDiv, LAnd, LOr };
  const unsigned char SOME_MAGIC_VALUES[] = /*pp for W.vvvv.L.pp*/
        { 0x03, 0x03, 0x03, 0x03, 0x01, 0x01 };
  const unsigned char OPCODES[] =
      {
          /*VADDSD*/ 0x58, /*VMULSD*/ 0x59, /*VSUBSD*/ 0x5C,
          /*VDIVSD*/ 0x5E, /*VANDPD*/ 0x54, /*VORPD */ 0x56,
      };


  static bool GenerateBinaryOperator(Context *context,
                                     X86Code *code,
                                     const llvm::Instruction *inst,
                                     InstructionType type,
                                     size_t blockIndex);

  typedef bool Generator(Context *context, X86Code *code, const llvm::Instruction *inst, size_t blockIndex);

  static Generator GenerateAssignmentOperator;
  static Generator GenerateCallInstruction   ;
  static Generator GenerateFCmpInstruction   ;
  static Generator GenerateBrInstruction     ;
  static Generator GenerateRetInstruction    ;

  static void GenerateBasicBlock(Context *context, const llvm::BasicBlock *block, X86Code *code, size_t blockIndex);

  static bool GenerateGlobalVariable
      (Module *theModule, X86Code *code, GlobalContext *context);

  static void GenerateFunction
    (GlobalContext *globalContext, const llvm::Function *function, X86Code *code);

  static void GenerateProlog(Context *context, X86Code *code, const llvm::Function *function);

  static bool GenerateStandardLibrary(GlobalContext *context, X86Code *code);
  X86Code *GenerateX86CodeFromModule(Module *theModule)
  {
    assert(theModule);

    X86Code *code =
        (X86Code *) calloc(1, sizeof(X86Code));
    if (!code) OUT_OF_MEMORY(return nullptr);

    GlobalContext globalContext{};
    GenerateGlobalVariable(theModule, code, &globalContext);

    for (const llvm::Function &function : *theModule->theModule)
      GenerateFunction(&globalContext, &function, code);

    GenerateStandardLibrary(&globalContext, code);

    UpdateCallReferences(&globalContext, code);
    code->flashing = globalContext.flashing;
    DestroyGlobalContext(&globalContext);
    return code;
  }

  static bool GenerateStandardLibrary(GlobalContext *context, X86Code *code)
  {
    assert(context && code);

    context->flashing.libOffset = code->text.size;

    #define WRITE_FUNCTION(NAME)                       \
      do {                                             \
        AddCallLabel(context, #NAME, code->text.size); \
        unsigned char NAME[] =                         \
            { XOR_RAX_RAX_DATA, RET_DATA };            \
        Write(code, NAME, sizeof(NAME));               \
      } while (false)

    WRITE_FUNCTION(sin);
    WRITE_FUNCTION(cos);
    WRITE_FUNCTION(tan);
    WRITE_FUNCTION(pow);
    AddCallLabel(context, "sqrt", code->text.size);
    unsigned char sqrt[] = { SQRT_DATA };
    Write(code, sqrt, sizeof(sqrt));
    AddCallLabel(context, "printString", code->text.size);
    unsigned char printString[] = { PRINT_STRING_DATA };
    Write(code, printString, sizeof(printString));
    AddCallLabel(context, "printDouble", code->text.size);
    unsigned char printDouble[] = { PRINT_DOUBLE_DATA };
    Write(code, printDouble, sizeof(printDouble));
    AddCallLabel(context, "scanDouble", code->text.size);
    unsigned char scanDouble[] = { SCAN_DOUBLE_DATA };
    Write(code, scanDouble, sizeof(scanDouble));

    context->flashing.libSize =
        code->text.size - context->flashing.libOffset;
    return true;
  }

  static bool CreateContext(Context *context, size_t blocksCount, GlobalContext *globalContext)
    {
      assert(context && globalContext);

      *context = {};
      context->varTables =
          (BlockVariableTable *) calloc(blocksCount, sizeof(BlockVariableTable));
      if (!context->varTables) OUT_OF_MEMORY(return false);
      context->blocksCount = blocksCount;
      context->globalContext = globalContext;

      return true;
    }

  static bool DestroyGlobalContext(GlobalContext *context)
  {
    assert(context);

    free(context->globalVarTable.doubles.data);
    free(context->globalVarTable.strings.data);

    *context = {};
    return true;
  }
  static bool DestroyContext(Context *context)
  {
    assert(context);

    free(context->jumpRefTable.data);
    free(context->jumpLabelTable.data);
    for (size_t i = 0; i < context->multiVarTable.size; ++i)
      free(context->multiVarTable.data[i].useVar);
    free(context->multiVarTable.data);
    for (size_t i = 0; i < context->blocksCount; ++i)
      free(context->varTables[i].data);
    free(context->varTables);

    *context = {};
    return true;
  }

  static void GenerateProlog(Context *context, X86Code *code, const llvm::Function *function)
  {
    assert(context && code && function);

    unsigned  char data[] =
        { PUSH_RBP_DATA, MOV_RBP_RSP_DATA, SUB_RSP_IMM_DATA  };
    WRITE_INT32(&data[7], context->varsCount*sizeof(double));
    Write(code, data, sizeof(data));

    size_t size = function->arg_size();
    for (size_t i = 0; i < size; ++i)
      {
        size_t offset = i*sizeof(double)+sizeof(double);
        WRITE_MOV_STACK_REG(-offset, i);
      }
    WRITE_PUSH_XMMS();
  }

  static bool GenerateGlobalVariable
    (Module *theModule, X86Code *code, GlobalContext *context)
  {
    assert(theModule && code && context);
    GlobalVariableTable *table = &context->globalVarTable;

    size_t globalSectionSize = 0;
    size_t stringSectionSize = 0;
    size_t stringCount = 0;
    for (const auto &globalVar : theModule->theModule->globals())
      {
        llvm::Type *type = globalVar.getValueType();
        switch (type->getTypeID())
        {
          case llvm::Type::DoubleTyID:
            { globalSectionSize += sizeof(double); break; }
          case llvm::Type::ArrayTyID:
            {
              ++stringCount;
              llvm::Type *elementType =
                  type->getArrayElementType();
              if (!elementType->isIntegerTy(8))
                {
                  fprintf(stderr,
                          "Unknown array type: %d. File: \"%s\", Line: %d.\n",
                          elementType->getTypeID(), __FILE__, __LINE__);
                  return false;
                }

              stringSectionSize +=
                  type->getArrayNumElements();
              break;
            }
          default:
            {
              fprintf(stderr,
                      "Unknown type: %d. File: \"%s\", Line: %d.\n",
                      type->getTypeID(), __FILE__, __LINE__);
              return false;
            }
        }
      }

    code->data.data = globalSectionSize ?
        (std::byte *) calloc(globalSectionSize, sizeof(char)) : nullptr;
    if (globalSectionSize && !code->data.data)
      OUT_OF_MEMORY(return false);

    code->rodata.data = stringSectionSize ?
        (std::byte *) calloc(stringSectionSize, sizeof(char)) : nullptr;
    if (stringSectionSize && !code->rodata.data)
      OUT_OF_MEMORY(free(code->data.data); return false);

    table->doubles.size = globalSectionSize/sizeof(double);
    table->doubles.data =
        (GlobalVariable *) calloc(table->doubles.size, sizeof(GlobalVariable));
    if (table->doubles.size && !table->doubles.data)
      OUT_OF_MEMORY(free(code->data.data); free(code->rodata.data); return false);

    table->strings.size = stringCount;
    table->strings.data =
        (GlobalVariable *) calloc(table->strings.size, sizeof(GlobalVariable));
    if (table->strings.size && !table->strings.data)
      OUT_OF_MEMORY(free(code->data.data); free(code->rodata.data); free(table->doubles.data); return false);

    size_t globalIndex = 0;
    size_t stringIndex = 0;
    stringCount = 0;
    for (const auto &globalVar : theModule->theModule->globals())
      {
        llvm::Type *type = globalVar.getValueType();
        switch (type->getTypeID())
        {
          case llvm::Type::DoubleTyID:
            {
              double value =
                  ((llvm::ConstantFP *)globalVar.getInitializer())->getValue().convertToDouble();
              table->doubles.data[globalIndex] =
                  { globalVar.getName().data(), globalIndex*sizeof(double), sizeof(double) };
              ((double *) code->data.data)[globalIndex++] = value;
              break;
            }
          case llvm::Type::ArrayTyID:
            {
              const char *string =
                  ((llvm::ConstantDataArray *) globalVar.getInitializer())->getRawDataValues().data();
              size_t size = strlen(string);
              table->strings.data[stringCount++] =
                  { globalVar.getName().data(), stringIndex, size };
              strcpy((char *) code->rodata.data + stringIndex, string);
              stringIndex += type->getArrayNumElements();
              break;
            }
          default: break;
        }
      }

    code->  data.size = code->  data.capacity = globalSectionSize;
    code->rodata.size = code->rodata.capacity = stringSectionSize;
    return true;
  }
  static void GenerateFunction
    (GlobalContext *globalContext, const llvm::Function *function, X86Code *code)
  {
    assert(globalContext && function && code);
    if (function->empty()) return;
    AddCallLabel(globalContext, function->getName().data(), code->text.size);

    Context context{};
    CreateContext(&context, function->size(), globalContext);
    if (!strcmp("main", function->getName().data()))
      {
        globalContext->flashing.mainAddress = code->text.size;
        globalContext->flashing.  data =
            { code->text.size + 2 };
        WRITE_MOVABS_REG(R15, 0);
        globalContext->flashing.rodata =
            { code->text.size + 2 };
        WRITE_MOVABS_REG(R13, 0);
        context.inMain = true;
      }

    GenerateVariableTables(&context, function);
    GenerateProlog(&context, code, function);

    size_t index = 0;
    for (const llvm::BasicBlock &block : *function)
      GenerateBasicBlock(&context, &block, code, index++);

    UpdateJumpReferences(&context, code);
    DestroyContext(&context);
  }
  static void GenerateBasicBlock(Context *context, const llvm::BasicBlock *block, X86Code *code, size_t blockIndex)
    {
      assert(context && block && code);

      for (size_t i = 0; i < context->varTables[blockIndex].size; ++i)
        if (isalpha(context->varTables[blockIndex].data[i].name[0]))
          {
            const char *name =
                context->varTables[blockIndex].data[i].name;
            location_t location =
                context->varTables[blockIndex].data[i].location;
            size_t offset =
                context->varTables[blockIndex].data[i].offset;
            size_t j = 0;
            for ( ; j < context->multiVarTable.size; ++j)
              if (!strcmp(name, context->multiVarTable.data[j].name))
                break;
            WRITE_MOV_REG_STACK(RAX, -j*sizeof(double) - 8);
            if (location < mem)
              WRITE_VMOVQ_XMM_REG(location, RAX);
            else
              WRITE_MOV_STACK_REG(offset, RAX);
          }
      AddJumpLabel(context, block->getName().data(), code->text.size);

      for (const llvm::Instruction &inst : *block)
        {
          switch (inst.getOpcode())
          {
            #define CASE(OP_CODE)                                                               \
              case llvm::Instruction::OP_CODE:                                                  \
                { Generate ## OP_CODE ## Instruction(context, code, &inst, blockIndex); } break

            CASE(FAdd); CASE(FSub); CASE(FMul); CASE(FDiv); CASE(And); CASE(Or);
            CASE(FCmp);
            CASE(Load); CASE(Store); /*CASE(Alloca); - do nothing*/
            CASE(Call);
            CASE(Br); CASE(Ret);

            default: break;
            #undef CASE
          }
        }

      for (size_t i = 0; i < context->varTables[blockIndex].size; ++i)
        if (isalpha(context->varTables[blockIndex].data[i].name[0]))
          {
            const char *name =
                context->varTables[blockIndex].data[i].name;
            location_t location =
                context->varTables[blockIndex].data[i].location;
            size_t offset =
                context->varTables[blockIndex].data[i].offset;
            size_t j = 0;
            for ( ; j < context->multiVarTable.size; ++j)
              if (!strcmp(name, context->multiVarTable.data[j].name))
                break;
            if (location < mem)
              WRITE_VMOVQ_REG_XMM(RAX, location);
            else
              WRITE_MOV_REG_STACK(RAX, offset);
            WRITE_MOV_STACK_REG(-j*sizeof(double), RAX);
          }
    }

  static void PrepareArguments(Context *context, X86Code *code, llvm::Value **values, size_t size, location_t *locations, size_t blockIndex);
  static void CleanupArguments(Context *context, X86Code *code, llvm::Value **values, size_t size, location_t *locations, size_t blockIndex);

  static bool GenerateFCmpInstruction(Context *context, X86Code *code, const llvm::Instruction *inst, size_t blockIndex)
  {
    assert(context && code && inst);
    llvm::FCmpInst *cmpInst = (llvm::FCmpInst *) inst;

    llvm::Value *values[3] =
        { (llvm::Value *) inst, inst->getOperand(0), inst->getOperand(1) };
    location_t locations[3] = {};
    PrepareArguments(context, code, values, 3, locations, blockIndex);
    unsigned char type = 0;
    switch (cmpInst->getPredicate())
    {
      case llvm::CmpInst::FCMP_OEQ: { type = EQ; break; }
      case llvm::CmpInst::FCMP_ONE: { type = NE; break; }
      case llvm::CmpInst::FCMP_OLT: { type = GT; break; }
      case llvm::CmpInst::FCMP_OGT: { type = LT; break; }
      default: break;
    }
    WRITE_VCMPSD(locations[0], locations[1], locations[2], type);
    CleanupArguments(context, code, values, 2, locations, blockIndex);
    return true;
  }
  static bool GenerateAssignmentOperator(Context *context, X86Code *code, const llvm::Instruction *inst, size_t blockIndex)
  {
    assert(context && code && inst);

    llvm::Value *values[2] = {};
    if (llvm::isa<llvm::LoadInst>(inst))
      { values[0] = (llvm::Value *) inst; values[1] = inst->getOperand(0); }
    else
      { values[0] =  inst->getOperand(0); values[1] = inst->getOperand(1); }
    location_t locations[2] = {};
    PrepareArguments(context, code, values, 2, locations, blockIndex);

    SecondByte secondByte{ 1, locations[1] < xmm8, 1, locations[0] < xmm8 };
    FifthByte fifthByte{ (unsigned char) locations[1], (unsigned char) locations[0], 0x3 };
    unsigned char data[] =
        { 0xC4, BYTE(secondByte), 0x7A, 0x7E, BYTE(fifthByte) };
    Write(code, data, sizeof(data));

    CleanupArguments(context, code, values, 2, locations, blockIndex);
    return true;
  }
  static bool GenerateBrInstruction(Context *context, X86Code *code, const llvm::Instruction *inst, size_t blockIndex)
  {
    assert(context && code && inst);
    llvm::BranchInst *branchInst = (llvm::BranchInst *) inst;

    if (!branchInst->isConditional())
      {
        AddJumpReference(context,
                         code->text.size,
                         code->text.size + 1,
                         -5,
                         branchInst->getOperand(0)->getName().data());
        unsigned char data[] = { JMP_DATA };
        Write(code, data, sizeof(data));
        return true;
      }
    const char *thenLabel = branchInst->getOperand(2)->getName().data();
    const char *elseLabel = branchInst->getOperand(1)->getName().data();

    llvm::Value *value = branchInst->getOperand(0);
    if (value && llvm::isa<llvm::ConstantFP>(value))
      WRITE_MOVABS_REG(RAX, ((llvm::ConstantFP *) value)->getValue().convertToDouble());
    else if (value && llvm::isa<llvm::GlobalVariable>(value))
      WRITE_MOV_REG_MEM(RAX, GetGlobalOffset(context, value->getName().data()));
    else if (value)
      {
        location_t location =
            GetVariableLocation(context, value->getName().data(), blockIndex);
        if (location < mem) WRITE_VMOVQ_REG_XMM(RAX, location);
        else WRITE_MOV_REG_STACK(RAX, GetVariableOffset(context, value->getName().data(), blockIndex));
      }

    AddJumpReference(context,code->text.size + 3, code->text.size +  5, -6, thenLabel);
    AddJumpReference(context,code->text.size + 9, code->text.size + 10, -5, elseLabel);
    unsigned char data[] =
        { TEST_RAX_RAX_DATA, JZ_DATA, JMP_DATA };
    Write(code, data, sizeof(data));
    return true;
  }
  static bool GenerateRetInstruction(Context *context, X86Code *code, const llvm::Instruction *inst, size_t blockIndex)
  {
    assert(context && code  && inst);
    llvm::ReturnInst *returnInst = (llvm::ReturnInst *) inst;

    llvm::Value *value = returnInst->getReturnValue();
    if (value && llvm::isa<llvm::ConstantFP>(value))
      WRITE_MOVABS_REG(RAX, ((llvm::ConstantFP *) value)->getValue().convertToDouble());
    else if (value && llvm::isa<llvm::GlobalVariable>(value))
      WRITE_MOV_REG_MEM(RAX, GetGlobalOffset(context, value->getName().data()));
    else if (value)
      {
        location_t location =
            GetVariableLocation(context, value->getName().data(), blockIndex);
        if (location < mem) WRITE_VMOVQ_REG_XMM(RAX, location);
        else WRITE_MOV_REG_STACK(RAX, GetVariableOffset(context, value->getName().data(), blockIndex));
      }

    WRITE_POP_XMMS();
    unsigned char stackData[] = { ADD_RSP_IMM_DATA, POP_RBP_DATA };
    WRITE_INT32(&stackData[3], context->varsCount*sizeof(double));
    Write(code, stackData, sizeof(stackData));
    if (context->inMain)
      {
        unsigned char exitData[] =
            { XOR_EDI_EDI_DATA, MOV_EAX_3C, SYSCALL_DATA };
        Write(code, exitData, sizeof(exitData));
      }
    else
      {
        unsigned char retData[] = { RET_DATA };
        Write(code, retData, sizeof(retData));
      }
    return true;
  }
  static bool GenerateCallInstruction(Context *context, X86Code *code, const llvm::Instruction *inst, size_t blockIndex)
  {
    assert(context && code && inst);
    llvm::CallInst *callInst = (llvm::CallInst *) inst;

    unsigned char _data[] = { 0x90 };
    Write(code, _data, sizeof(_data));

    llvm::Value *args[13] = {};
    location_t locations[13] = {};
    size_t size = callInst->getNumOperands() - 1;
    args[0] = (llvm::Value *) inst;
    for (size_t i = 0; i < size; ++i) args[i + 1] = callInst->getOperand(i);
    PrepareArguments(context, code, args, size + 1, locations, blockIndex);

    unsigned char regIndex = 0;
    for (size_t i = 0; i < size; ++i)
      {
        WRITE_PUSH_REG(regIndex);
        WRITE_VMOVQ_REG_XMM(regIndex, locations[i+1]);
        if (++regIndex == RSP) regIndex += 2;
      }

    const char *name = callInst->getCalledFunction()->getName().data();
    AddCallReference(context->globalContext, code->text.size, code->text.size + 1, -5, name);
    unsigned char data[] = { CALL_DATA };
    Write(code, data, sizeof(data));
    WRITE_VMOVQ_XMM_REG(locations[0], RAX);

    for (size_t i = size; i > 0; --i)
      {
        if (--regIndex == RSI) regIndex -= 2;
        WRITE_POP_REG(regIndex);
      }

    CleanupArguments(context, code, args, size, locations, blockIndex);
    Write(code, _data, sizeof(_data));
    return true;
  }

  static bool GenerateBinaryOperator(Context *context,
                                     X86Code *code,
                                     const llvm::Instruction *inst,
                                     InstructionType type,
                                     size_t blockIndex)
  {
    assert(context && code && inst);

    llvm::Value *values[3] =
        { (llvm::Value *) inst, inst->getOperand(0), inst->getOperand(1) };
    location_t locations[3] = {};
    PrepareArguments(context, code, values, 3, locations, blockIndex);

    SecondByte secondByte { 1, locations[2] < xmm8, true, locations[0] < xmm8 };
    ThirdByte thirdByte { SOME_MAGIC_VALUES[type], false, (unsigned char)(15 - locations[1]), false };
    FifthByte fifthByte
    {
      (unsigned char)(locations[0] - (!secondByte.B)*8),
      (unsigned char)(locations[2] - (!secondByte.R)*8),
      0x3
    };

    unsigned char data[CMD_SIZE] =
        { VEX_PREFIX, BYTE(secondByte), BYTE(thirdByte), OPCODES[type], BYTE(fifthByte) };
    Write(code, data, sizeof(data));
    CleanupArguments(context, code, values, 3, locations, blockIndex);
    return true;
  }

  typedef bool PushFunction(Context *context, const char *name, size_t blockIndex);
  static PushFunction PushRealVariable;
  static PushFunction     PushVariable;
  static bool CalculateVarLocations(Context *context, size_t blockIndex);
  static bool ParseInstruction(Context *context,
                               PushFunction *pushFunction,
                               const llvm::Instruction *inst,
                               int (*checkName)(int ch),
                               size_t blockIndex);

  static bool ParseInstruction(Context *context,
                               PushFunction *pushFunction,
                               const llvm::Instruction *inst,
                               int (*checkName)(int ch),
                               size_t blockIndex)
  {
    assert(pushFunction && inst && checkName);

    switch (inst->getOpcode())
    {
      #define CASE(OP_CODE) case llvm::Instruction::OP_CODE

      CASE(FAdd): CASE(FSub): CASE(FMul): CASE(FDiv): CASE(And): CASE(Or): CASE(FCmp):
      CASE(Load): CASE(Store):
      CASE(Alloca):
        {
          size_t count = inst->getNumOperands();

          llvm::Value *operand0 = (llvm::Value *) inst;
          if (operand0 &&
              !llvm::isa<llvm::GlobalVariable>(operand0) &&
              !llvm::isa<llvm::ConstantFP>    (operand0) &&
                  checkName(operand0->getName().data()[0]))
            pushFunction(context, operand0->getName().data(), blockIndex);

          llvm::Value *operand1 = inst->getOperand(0);
          if (operand1 &&
              !llvm::isa<llvm::GlobalVariable>(operand1) &&
              !llvm::isa<llvm::ConstantFP>    (operand1) &&
              checkName(operand0->getName().data()[0]))
            pushFunction(context, operand1->getName().data(), blockIndex);

          llvm::Value *operand2 = count > 1 ? inst->getOperand(1) : nullptr;
          if (operand2 &&
              !llvm::isa<llvm::GlobalVariable>(operand2) &&
              !llvm::isa<llvm::ConstantFP>    (operand2) &&
              checkName(operand0->getName().data()[0]))
            pushFunction(context, operand2->getName().data(), blockIndex);
          break;
        }
      CASE(Call):
        {
          llvm::CallInst *callInst = (llvm::CallInst *) inst;
          if (!callInst->getType()->isVoidTy())
            {
              llvm::Value *operand = (llvm::Value *) inst;
              pushFunction(context, operand->getName().data(), blockIndex);
            }

          size_t size = callInst->getNumOperands();
          for (size_t index = 0; index < size; ++index)
            {
              llvm::Value *operand = inst->getOperand(index++);
              if (!llvm::isa<llvm::GlobalVariable>(operand) &&
                  !llvm::isa<llvm::ConstantFP>    (operand))
                pushFunction(context, operand->getName().data(), blockIndex);
            }
          break;
        }
      CASE(Br): CASE(Ret): default: break;

      #undef CASE
    }

    return true;
  }

  static bool PushRealVariable(Context *context, const char *name, size_t blockIndex)
  {
    assert(context && name);

    MultiBlocksVariableTable *table = &context->multiVarTable;
    for (size_t i = 0; i < table->size; ++i)
      if (!strcmp(name, table->data[i].name)) return true;

    if (table->size == table->capacity)
      {
        size_t capacity = 2*table->capacity+1;
        MultiBlocksVariable *temp =
            (MultiBlocksVariable *) realloc(table->data, capacity*sizeof(MultiBlocksVariable));
        if (!temp) OUT_OF_MEMORY(return false);
        table->data = temp;
        table->capacity = capacity;
      }

    table->data[table->size++] = { name };
    return true;
  }
  static bool PushVariable(Context *context, const char *name, size_t blockIndex)
  {
    assert(context && name);

    BlockVariableTable *table = &context->varTables[blockIndex];
    for (size_t i = 0; i < table->size; ++i)
      if (!strcmp(name, table->data[i].name))
        { ++table->data[i].usageCount; return true; }

    if (table->size == table->capacity)
      {
        size_t capacity = 2*table->capacity+1;
        BlockVariable *temp =
            (BlockVariable *) realloc(table->data, capacity*sizeof(BlockVariable));
        if (!temp) OUT_OF_MEMORY(return false);
        table->data = temp;
        table->capacity = capacity;
      }

    table->data[table->size++] = { name, 1 };
    return true;
  }

  static bool CalculateVarLocations(Context *context, size_t blockIndex)
  {
    assert(context);

    size_t offset = sizeof(double)*context->multiVarTable.size;
    BlockVariableTable *table = &context->varTables[blockIndex];
    for (size_t i = 0; i < table->size; ++i)
      {
        table->data[i].location =
            i < xmm15 ? (location_t) i : mem;
        if (i < xmm15) continue;

        offset += sizeof(double);
        table->data[i].offset = offset;
      }

    return true;
  }

  static void GenerateVariableTables
    (Context *context, const llvm::Function *function)
  {
    assert(context && function);

    for (size_t i = 0; i < function->arg_size(); ++i)
      PushRealVariable(context, function->getArg(i)->getName().data(), 0);

    size_t index = 0;
    for (const llvm::BasicBlock &block : *function)
      {
        for (const llvm::Instruction &inst : block)
          {
            ParseInstruction(context, PushRealVariable, &inst, isalpha, 0);
            ParseInstruction(context, PushVariable    , &inst, isalnum, index);
          }
          ++index;
      }

    for (size_t i = 0; i < context->blocksCount; ++i)
      CalculateVarLocations(context, i);

    size_t maxMemoryVarCount = 0;
    for (size_t i = 0; i < context->blocksCount; ++i)
      {
        size_t memoryVarCount = 0;
        for (size_t j = 0; j < context->varTables[i].size; ++j)
          if (context->varTables[i].data[j].location == mem) ++memoryVarCount;

        if (maxMemoryVarCount > memoryVarCount)
          maxMemoryVarCount = memoryVarCount;
      }

    context->varsCount =
        context->multiVarTable.size + maxMemoryVarCount;
  }

  static bool AddJumpLabel(Context *context, const char *name, size_t position)
  {
    assert(context && name);

    if (context->jumpLabelTable.size == context->jumpLabelTable.capacity)
      {
        size_t capacity = 2*context->jumpLabelTable.capacity + 1;
        JumpLabel *temp =
            (JumpLabel *) realloc(context->jumpLabelTable.data, capacity*sizeof(JumpLabel));
        if (!temp) OUT_OF_MEMORY(return false);

        context->jumpLabelTable.data = temp;
        context->jumpLabelTable.capacity = capacity;
      }

    size_t index = context->jumpLabelTable.size++;
    context->jumpLabelTable.data[index] = { name, position };
    return true;
  }
  static bool AddJumpReference(Context *context, size_t pos, size_t refPos, size_t delta, const char *label)
    {
      assert(context && label);

      if (context->jumpRefTable.size == context->jumpRefTable.capacity)
        {
          size_t capacity = 2*context->jumpRefTable.capacity + 1;
          JumpReference *temp =
              (JumpReference *) realloc(context->jumpRefTable.data, capacity*sizeof(JumpReference));
          if (!temp) OUT_OF_MEMORY(return false);

          context->jumpRefTable.data = temp;
          context->jumpRefTable.capacity = capacity;
        }

      size_t index = context->jumpRefTable.size++;
      context->jumpRefTable.data[index] = { pos, refPos, delta, label };
      return true;
    }
  static bool UpdateJumpReferences(Context *context, X86Code *code)
  {
    assert(context && code);

    for (size_t i = 0; i < context->jumpRefTable.size; ++i)
      {
        JumpReference *ref = &context->jumpRefTable.data[i];
        size_t position = 0;
        for (size_t j = 0; j < context->jumpLabelTable.size; ++j)
          if (!strcmp(ref->referee, context->jumpLabelTable.data[j].name))
            {
              position = context->jumpLabelTable.data[j].position;
              break;
            }

        size_t offset = position - ref->position + ref->delta;
        WRITE_INT32(&code->text.data[ref->referencePosition], offset);
      }

    return true;
  }

  static bool AddCallLabel(GlobalContext *context, const char *name, size_t position)
  {
    assert(context && name);

    if (context->callLabelTable.size == context->callLabelTable.capacity)
      {
        size_t capacity = 2*context->callLabelTable.capacity + 1;
        JumpLabel *temp =
            (JumpLabel *) realloc(context->callLabelTable.data, capacity*sizeof(JumpLabel));
        if (!temp) OUT_OF_MEMORY(return false);

        context->callLabelTable.data = temp;
        context->callLabelTable.capacity = capacity;
      }

    size_t index = context->callLabelTable.size++;
    context->callLabelTable.data[index] = { name, position };
    return true;
  }
  static bool AddCallReference(GlobalContext *context, size_t pos, size_t refPos, size_t delta, const char *label)
    {
      assert(context && label);

      if (context->callRefTable.size == context->callRefTable.capacity)
        {
          size_t capacity = 2*context->callRefTable.capacity + 1;
          JumpReference *temp =
              (JumpReference *) realloc(context->callRefTable.data, capacity*sizeof(JumpReference));
          if (!temp) OUT_OF_MEMORY(return false);

          context->callRefTable.data = temp;
          context->callRefTable.capacity = capacity;
        }

      size_t index = context->callRefTable.size++;
      context->callRefTable.data[index] = { pos, refPos, delta, label };
      return true;
    }
  static bool UpdateCallReferences(GlobalContext *context, X86Code *code)
  {
    assert(context && code);

    for (size_t i = 0; i < context->callRefTable.size; ++i)
      {
        JumpReference *ref = &context->callRefTable.data[i];
        size_t position = 0;
        for (size_t j = 0; j < context->callLabelTable.size; ++j)
          if (!strcmp(ref->referee, context->callLabelTable.data[j].name))
            {
              position = context->callLabelTable.data[j].position;
              break;
            }

        size_t offset = position - ref->position + ref->delta;
        WRITE_INT32(&code->text.data[ref->referencePosition], offset);
      }

    return true;
  }


  static void PrepareArguments(Context *context, X86Code *code, llvm::Value **values, size_t size, location_t *locations, size_t blockIndex)
  {
    assert(context && values && locations && size <= REG_COUNT);

    const size_t DEFAULT_REGS = 0;
    const size_t XMM_REGS = 1;
    unsigned char regIndex[2] = {};
    bool isRegisterUsing[2][REG_COUNT] = {};
    isRegisterUsing[DEFAULT_REGS][RSP] = true;
    isRegisterUsing[DEFAULT_REGS][RBP] = true;
    isRegisterUsing[DEFAULT_REGS][R15] = true;

    for (size_t i = 0; i < size; ++i)
      if (!llvm::isa<llvm::ConstantFP>(values[i]) && !llvm::isa<llvm::GlobalVariable>(values[i]))
        {
          if (!values[i]->getName().data()[0]) continue;
          location_t location =
              GetVariableLocation(context, values[i]->getName().data(), blockIndex);
          if (location != mem) isRegisterUsing[XMM_REGS][location] = true;
        }

    #define EMIT_XMM(PREPARING, GETTING_VALUE)          \
        do {                                            \
          PREPARING                                     \
          locations[i] = (location_t) regIndex[XMM_REGS]; \
          WRITE_VMOVQ_REG_XMM(regIndex[0], regIndex[XMM_REGS]);\
          GETTING_VALUE                                 \
          WRITE_VMOVQ_XMM_REG(regIndex[XMM_REGS], R14); \
          isRegisterUsing[DEFAULT_REGS][regIndex[DEFAULT_REGS]] = true; \
          isRegisterUsing[XMM_REGS][regIndex[XMM_REGS]] = true;\
        } while (false)

    for (size_t i = 0; i < size; ++i)
      {
        for ( ; regIndex[DEFAULT_REGS] < REG_COUNT; ++regIndex[DEFAULT_REGS])
          if (!isRegisterUsing[DEFAULT_REGS][regIndex[DEFAULT_REGS]]) break;
        for ( ; regIndex[XMM_REGS    ] < REG_COUNT; ++regIndex[XMM_REGS    ])
          if (!isRegisterUsing[XMM_REGS    ][regIndex[XMM_REGS    ]]) break;

        llvm::Value *value = values[i];
        if (!llvm::isa<llvm::GlobalVariable>(value) &&
            !llvm::isa<llvm::ConstantFP>(value) &&
            !value->getName().data()[0]) continue;

        if (llvm::isa<llvm::GlobalVariable>(value))
          {
            if (value->getType()->isDoubleTy())
              EMIT_XMM(
                size_t offset =
                  GetGlobalOffset(context, value->getName().data());,
                WRITE_MOV_REG_MEM(R14, offset);
                );
            else
              EMIT_XMM(
                size_t offset =
                  GetGlobalOffset(context, value->getName().data());,
                WRITE_MOVABS_REG(R14, offset);
                );
          }
        else if (llvm::isa<llvm::ConstantFP>(value))
          EMIT_XMM(
              double constValue =
                  ((llvm::ConstantFP *) value)->getValue().convertToDouble();,
              WRITE_MOVABS_REG(R14, constValue);
              );
        else
          {
            location_t location =
                GetVariableLocation(context, value->getName().data(), blockIndex);
            if (location != mem) locations[i] = location;
            else
              EMIT_XMM(
                  size_t offset =
                      GetVariableOffset(context, value->getName().data(), blockIndex);,
                  WRITE_MOV_REG_STACK(R14, offset);
                  );
          }
      }
  }
  static void CleanupArguments(Context *context, X86Code *code, llvm::Value **values, size_t size, location_t *locations, size_t blockIndex)
  {
    assert(context && values && locations && size <= REG_COUNT);

    const size_t DEFAULT_REGS = 0;
    const size_t XMM_REGS = 1;
    unsigned char regIndex[2] = {};
    bool isRegisterUsing[2][REG_COUNT] = {};
    isRegisterUsing[DEFAULT_REGS][RSP] = true;
    isRegisterUsing[DEFAULT_REGS][RBP] = true;
    isRegisterUsing[DEFAULT_REGS][R15] = true;

    for (size_t i = 0; i < size; ++i)
      if (!llvm::isa<llvm::ConstantFP>(values[i]) && !llvm::isa<llvm::GlobalVariable>(values[i]))
        {
          if (!values[i]->getName().data()[0]) continue;

          location_t location =
              GetVariableLocation(context, values[i]->getName().data(), blockIndex);
          if (location != mem) isRegisterUsing[XMM_REGS][location] = true;
        }

    #define EMIT_XMM(PREPARING, PUTTING_VALUE)          \
        do {                                            \
          PREPARING                                     \
          WRITE_VMOVQ_REG_XMM(R14, regIndex[XMM_REGS]);        \
          PUTTING_VALUE                                 \
          WRITE_VMOVQ_XMM_REG(regIndex[XMM_REGS], regIndex[DEFAULT_REGS]);\
          isRegisterUsing[DEFAULT_REGS][regIndex[DEFAULT_REGS]] = true;       \
          isRegisterUsing[XMM_REGS][regIndex[XMM_REGS]] = true; \
        } while (false)

    for (size_t i = 0; i < size; ++i)
      {
        for ( ; regIndex[DEFAULT_REGS] < REG_COUNT; ++regIndex[DEFAULT_REGS])
          if (!isRegisterUsing[DEFAULT_REGS][regIndex[DEFAULT_REGS]]) break;
        for ( ; regIndex[XMM_REGS    ] < REG_COUNT; ++regIndex[XMM_REGS    ])
          if (!isRegisterUsing[XMM_REGS    ][regIndex[XMM_REGS    ]]) break;

        llvm::Value *value = values[i];
        if (!value->getName().data()[0]) continue;

        if (llvm::isa<llvm::GlobalVariable>(value))
          EMIT_XMM(
            size_t offset =
              GetGlobalOffset(context, value->getName().data());,
            WRITE_MOV_MEM_REG(offset, R14);
            );
        else if (llvm::isa<llvm::ConstantFP>(value))
          EMIT_XMM(,);
        else
          {
            location_t location =
                GetVariableLocation(context, value->getName().data(), blockIndex);
            if (location == mem)
              EMIT_XMM(
                  size_t offset =
                      GetVariableOffset(context, value->getName().data(), blockIndex);,
                  WRITE_MOV_STACK_REG(offset, R14);
                  );
          }
      }
  }

  static location_t GetVariableLocation(Context *context, const char *name, size_t blockIndex)
  {
    assert(context && name);

    BlockVariableTable *table = &context->varTables[blockIndex];
    for (size_t i = 0; i < table->size; ++i)
      if (!strcmp(name, table->data[i].name)) return table->data[i].location;
    return mem;
  }
  static size_t GetVariableOffset(Context *context, const char *name, size_t blockIndex)
  {
    assert(context && name);

    BlockVariableTable *table = &context->varTables[blockIndex];
    for (size_t i = 0; i < table->size; ++i)
      if (!strcmp(name, table->data[i].name)) return table->data[i].offset;
    return 0;
  }
  static size_t GetGlobalOffset(Context *context, const char *name)
  {
    assert(context && name);

    size_t offset = 0;
    GlobalVariableTable *table = &context->globalContext->globalVarTable;
    for (size_t i = 0; i < table->doubles.size; ++i)
      if (!strcmp(name, table->doubles.data[i].name)) return -table->doubles.data[i].offset;
    offset = 0;
    for (size_t i = 0; i < table->strings.size; ++i)
      if (!strcmp(name, table->strings.data[i].name)) return table->strings.data[i].offset;
    return 0;
  }
  static size_t GetVariableOffset(Context *context, const char *name)
  {
    assert(context && name);

    for (size_t i = 0; i < context->blocksCount; ++i)
      for (size_t j = 0; j < context->varTables[i].size; ++j)
        if (!strcmp(name, context->varTables[i].data[j].name))
          return context->varTables[i].data[j].offset;
    return 0;
  }
  static bool SearchVariable(Context *context, const char *name, location_t *location)
  {
    assert(context && name && location);

    for (size_t i = 0; i < context->blocksCount; ++i)
      for (size_t j = 0; j < context->varTables[i].size; ++j)
        if (!strcmp(name, context->varTables[i].data[j].name))
          {
            *location = context->varTables[i].data[j].location;
            return true;
          }
    return false;
  }

}