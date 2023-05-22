#include "CodeGen/x86Code.h"

#include "Utils/ErrorMessage.h"
#include "Utils/FreeAll.h"

#include <cstring>
#include <malloc.h>
#include <cassert>

namespace db {

  #define WRITE_INT64(ADR, VALUE) *(int64_t *) (ADR) = (int64_t) (VALUE)
  #define WRITE_INT32(ADR, VALUE) *(int32_t *) (ADR) = (int32_t) (VALUE)

  const size_t MAX_ARGS_COUNT = 6;

  static bool Write(x86Code *code, const void *buffer, size_t size);

  #include "Cmd.def"
  #include "x86CodeUtils.cpp.in"

  #define EMITTER(NAME) \
    static bool Emit ## NAME \
        (Context *context, const llvm::Instruction *inst, x86Code *code)

  EMITTER(Arithmetic);
  EMITTER(Assignment);
  EMITTER(FCmp ); EMITTER(Call );
  EMITTER(Br   ); EMITTER(Ret  );

  #undef EMITTER

  #define EmitFAdd(CONTEXT, INST, CODE) EmitArithmetic(CONTEXT, INST, CODE)
  #define EmitFSub(CONTEXT, INST, CODE) EmitArithmetic(CONTEXT, INST, CODE)
  #define EmitFMul(CONTEXT, INST, CODE) EmitArithmetic(CONTEXT, INST, CODE)
  #define EmitFDiv(CONTEXT, INST, CODE) EmitArithmetic(CONTEXT, INST, CODE)
  #define  EmitAnd(CONTEXT, INST, CODE) EmitArithmetic(CONTEXT, INST, CODE)
  #define   EmitOr(CONTEXT, INST, CODE) EmitArithmetic(CONTEXT, INST, CODE)

  #define   EmitLoad(CONTEXT, INST, CODE) EmitAssignment(CONTEXT, INST, CODE)
  #define  EmitStore(CONTEXT, INST, CODE) EmitAssignment(CONTEXT, INST, CODE)

  static bool EmitFunction
      (GlobalContext *globalContext, const llvm::Function *function, x86Code *code);
  static bool EmitMain
      (GlobalContext *globalContext, const llvm::Function *function, x86Code *code);
  static bool EmitBasicBlock
      (Context *context, const llvm::BasicBlock *block, x86Code *code);

  x86Code *GenerateX86Code(const Module *theModule)
    {
      assert(theModule);

      auto *code =
          (x86Code *) calloc(1, sizeof(x86Code));
      if (!code) OUT_OF_MEMORY(return nullptr);

      GlobalContext context{};
      CreateGlobalContext(&context, theModule, code);

      for (const llvm::Function &function : *theModule->theModule)
        EmitFunction(&context, &function, code);
      EmitStdLibrary(&context, code);

      UpdateCallReferences(&context, code);
      code->references =
          {
            context.references.writingRodataOffset,
            context.references.writingDataOffset
          };

      DestroyGlobalContext(&context);
      return code;
    }

  void DestroyX86Code(x86Code *code)
    {
      assert(code);
      FreeAll(code->text.data, code->data.data, code->rodata.data);
      *code = {};
    }

  static bool EmitFunction
      (GlobalContext *globalContext, const llvm::Function *function, x86Code *code)
    {
      assert(globalContext && function && code);
      if (function->empty()) return true;

      if (!strcmp("main", function->getName().data()))
        return EmitMain(globalContext, function, code);

      Context context{};
      CreateContext(&context, function);
      PushUsingRegisters(&context, function, code);
      for (const llvm::BasicBlock &block : *function)
        EmitBasicBlock(&context, &block, code);

      return true;
    }

  static bool EmitMain
      (GlobalContext *globalContext, const llvm::Function *function, x86Code *code)
    {
      assert(globalContext && function && code);

      globalContext->references.writingRodataOffset =
          code->text.size + MOVABS_OFFSET;
      Write(code, MOVABS_R15, sizeof(MOVABS_R15));
      globalContext->references.writingDataOffset   =
          code->text.size + MOVABS_OFFSET;
      Write(code, MOVABS_R14, sizeof(MOVABS_R14));

      Context context{};
      CreateContext(&context, function);
      context.status.inMain = true;
      for (const llvm::BasicBlock &block : *function)
        EmitBasicBlock(&context, &block, code);

      return true;
    }

  static bool EmitBasicBlock
      (Context *context, const llvm::BasicBlock *block, x86Code *code)
    {
      assert(context && block && code);

      for (const llvm::Instruction &inst : *block)
        switch (inst.getOpcode())
        {
          #define CASE(TYPE)                         \
            case llvm::Instruction::TYPE:            \
              {                                      \
                Emit ## TYPE (context, &inst, code); \
              } break

          CASE(FAdd ); CASE(FSub );
          CASE(FMul ); CASE(FDiv );
          CASE(And  ); CASE(Or   );
          CASE(FCmp ); CASE(Load );
          CASE(Store); CASE(Call );
          CASE(Br   ); CASE(Ret  );

          #undef CASE
        }

      return true;
    }

  static bool EmitArithmetic
      (Context *context, const llvm::Instruction *inst, x86Code *code)
    {
      assert(context && inst && code);

      size_t opcodeIndex = 0;
      switch (inst->getOpcode())
        {
          #define CASE(TYPE, VALUE) \
            case llvm::Instruction::TYPE: opcodeIndex = VALUE; break

          CASE(FAdd, vaddsd); CASE(FSub, vsubsd); CASE(FMul, vmulsd);
          CASE(FDiv, vdivsd); CASE(And , vandsd); CASE(Or  , vorsd );

          #undef CASE
        }
      Location locations[3]{};
      const llvm::Value *values[3] =
          { (llvm::Value *) inst, inst->getOperand(0), inst->getOperand(1) };

      GetValues(context, values, locations, 3, code);
      x86cmd5byte cmd =
          {
            VEX_VALUES[int(VEX::C4)],
            { X86CMD_MAP_SELECT, locations[2] < xmm8, X86CMD_X, locations[0] < xmm8 },
            {
              ARITHMETIC_OPCODES_EXTENSIONS[opcodeIndex],
              X86CMD_L,
              XMM_TO_VVVV(locations[1]),
              X86CMD_W
            },
            ARITHMETIC_OPCODES[opcodeIndex],
            {
              XMM_TO_ARG(locations[0]),
              XMM_TO_ARG(locations[2]),
              REG_REG_MOD
            }
          };
      Write(code, &cmd, VEX_SIZES[int(VEX::C4)]);
      CleanupValues(context, values, 3, code);
      return true;
    }

  static bool EmitRet
      (Context *context, const llvm::Instruction *inst, x86Code *code)
    {
      assert(context && inst && code);
      auto *returnInst =
          (llvm::ReturnInst *) inst;

      Location location{};
      const llvm::Value *returnValue =
          returnInst->getReturnValue();

      GetValues(context, &returnValue, &location, 1, code);
      x86cmd5byte cmd =
          {
            VEX_VALUES[int(VEX::C4)],
            { X86CMD_MAP_SELECT, rax < r8, X86CMD_X, location < xmm8 },
            { VMOVQ_REG_XMM_OPCODES_EXTENSIONS, X86CMD_L, VMOVQ_VVVV, X86CMD_W },
            VMOVQ_OPCODE,
            { REG_TO_ARG(rax), XMM_TO_ARG(location), REG_REG_MOD }
          };
      Write(code, &cmd, VEX_SIZES[int(VEX::C4)]);

      PopUsingRegisters(context, code);
      if (context->status.inMain)
        Write(code, MAIN_RET, sizeof(MAIN_RET));
      else
        Write(code,      RET, sizeof(     RET));
      return true;
    }

  static bool EmitAssignment
      (Context *context, const llvm::Instruction *inst, x86Code *code)
    {
      assert(context && inst && code);

      Location locations[2]{};
      const llvm::Value *values[2]{};
      if (llvm::isa<llvm::LoadInst>(inst))
        {
          values[0] = (llvm::Value *) inst;
          values[1] = inst->getOperand(0);
        }
      else
        {
          values[0] = inst->getOperand(0);
          values[1] = inst->getOperand(1);
        }

      GetValues(context, values, locations, 2, code);
      if (locations[1] < xmm8)
        {
          x86cmd4byte cmd =
              {
                VEX_VALUES[int(VEX::C5)],
                { VMOVQ_XMM_XMM_OPCODES_EXTENSIONS, X86CMD_L, VMOVQ_VVVV, locations[0] < xmm8 },
                VMOVQ_OPCODE,
                { XMM_TO_ARG(locations[1]), XMM_TO_ARG(locations[0]), REG_REG_MOD }
              };
          Write(code, &cmd, VEX_SIZES[int(VEX::C5)]);
        }
      else
        {
          x86cmd5byte cmd =
              {
                VEX_VALUES[int(VEX::C4)],
                { X86CMD_MAP_SELECT, VMOVQ_XMM_XMM_B, X86CMD_X, locations[0] < xmm8 },
                { VMOVQ_XMM_XMM_OPCODES_EXTENSIONS, X86CMD_L, VMOVQ_VVVV, X86CMD_W },
                VMOVQ_OPCODE,
                { XMM_TO_ARG(locations[1]), XMM_TO_ARG(locations[0]), REG_REG_MOD }
              };
          Write(code, &cmd, VEX_SIZES[int(VEX::C4)]);
        }
      CleanupValues(context, values, 2, code);
      return true;
    }

  static bool EmitFCmp
      (Context *context, const llvm::Instruction *inst, x86Code *code)
    {
      assert(context && inst && code);
      auto *cmpInst =
          (llvm::FCmpInst *) inst;

      size_t cmpTypeIndex = 0;
      switch (cmpInst->getPredicate())
        {
          #define CASE(TYPE, VALUE) \
            case llvm::CmpInst::TYPE: cmpTypeIndex = VALUE; break

          CASE(FCMP_OEQ, eq); CASE(FCMP_ONE, ne);
          CASE(FCMP_OGT, gt); CASE(FCMP_OLT, lt);

          #undef CASE

          default: break;
        }
      Location locations[3]{};
      const llvm::Value *values[3] =
          { (llvm::Value *) inst, inst->getOperand(0), inst->getOperand(1) };

      GetValues(context, values, locations, 3, code);

      if (locations[2] < xmm8)
        {
          x86cmd4byte cmd =
              {
                VEX_VALUES[int(VEX::C5)],
                { VCMPSD_OPCODES_EXTENSIONS, X86CMD_L, XMM_TO_VVVV(locations[1]), locations[0] < xmm8 },
                VCMPSD_OPCODE,
                { XMM_TO_ARG(locations[2]), XMM_TO_ARG(locations[0]), REG_REG_MOD }
              };
          Write(code, &cmd, VEX_SIZES[int(VEX::C5)]);
        }
      else
        {
          x86cmd5byte cmd =
              {
                VEX_VALUES[int(VEX::C4)],
                { X86CMD_MAP_SELECT, VCMPSD_B, X86CMD_X, locations[0] < xmm8 },
                { VCMPSD_OPCODES_EXTENSIONS, X86CMD_L, XMM_TO_VVVV(locations[1]), locations[0] < xmm8 },
                VCMPSD_OPCODE,
                { XMM_TO_ARG(locations[2]), XMM_TO_ARG(locations[0]), REG_REG_MOD }
              };
          Write(code, &cmd, VEX_SIZES[int(VEX::C4)]);
        }
      Write(code, &COMPARE_ARGUMENTS[cmpTypeIndex], sizeof(COMPARE_ARGUMENTS[cmpTypeIndex]));
      CleanupValues(context, values, 3, code);
      return true;
    }

  static bool EmitBr
      (Context *context, const llvm::Instruction *inst, x86Code *code)
    {
      assert(context && inst && code);
      auto *branchInst =
          (llvm::BranchInst *) inst;

      Location location{};
      const llvm::Value *value = inst->getOperand(0);

      if (!branchInst->isConditional())
        {
          Reference ref =
              {
                code->text.size,
                code->text.size + JMP_ADDRESS_OFFSET,
                sizeof(JMP),
                value->getName().data()
              };
          PushJumpReference(context, &ref);
          Write(code, JMP, sizeof(JMP));
          return true;
        }

      GetValues(context, &value, &location, 1, code);
      x86cmd5byte cmd =
          {
            VEX_VALUES[int(VEX::C4)],
            { X86CMD_MAP_SELECT, rax < r8, X86CMD_X, location < xmm8 },
            { VMOVQ_REG_XMM_OPCODES_EXTENSIONS, X86CMD_L, VMOVQ_VVVV, VMOVQ_REG_XMM_W },
            VMOVQ_OPCODE,
            { REG_TO_ARG(rax), XMM_TO_ARG(location), REG_REG_MOD }
          };
      Write(code, &cmd, VEX_SIZES[int(VEX::C4)]);
      Write(code, &CMP_EAX_EAX, sizeof(CMP_EAX_EAX));
      CleanupValues(context, &value, 3, code);

      Reference thenRef =
          {
            code->text.size,
            code->text.size + JNE_ADDRESS_OFFSET,
            sizeof(JNE),
            inst->getOperand(1)->getName().data()
          };
      PushJumpReference(context, &thenRef);
      Write(code, JNE, sizeof(JNE));
      Reference elseRef =
          {
            code->text.size,
            code->text.size + JMP_ADDRESS_OFFSET,
            sizeof(JMP),
            inst->getOperand(2)->getName().data()
          };
      PushJumpReference(context, &elseRef);
      Write(code, JMP, sizeof(JMP));
      return true;
    }

  static bool EmitCall
      (Context *context, const llvm::Instruction *inst, x86Code *code)
    {
      assert(context && inst && code);

      size_t argsCount =
          inst->getNumOperands() - 1;
      if (argsCount >= MAX_ARGS_COUNT) return false;

      Location locations[MAX_ARGS_COUNT + 1]{};
      const llvm::Value *values[MAX_ARGS_COUNT + 1] =
          { (llvm::Value *) inst };
      for (size_t i = 0; i < argsCount; ++i)
        values[i + 1] = inst->getOperand(i);
      GetValues(context, values, locations, argsCount, code);

      for (size_t i = 0; i < argsCount; ++i)
        {
          x86cmd5byte cmd =
              {
                VEX_VALUES[int(VEX::C4)],
                { X86CMD_MAP_SELECT, (Location) i < r8, X86CMD_X, locations[i] < xmm8 },
                { VMOVQ_REG_XMM_OPCODES_EXTENSIONS, X86CMD_L, VMOVQ_VVVV, VMOVQ_REG_XMM_W },
                VMOVQ_OPCODE,
                { REG_TO_ARG((Location) i), XMM_TO_ARG(locations[i]), REG_REG_MOD }
              };
          Write(code, &cmd, VEX_SIZES[int(VEX::C4)]);
        }

      Reference ref =
          {
            code->text.size,
            code->text.size + CALL_OFFSET,
            sizeof(CALL),
            inst->getOperand(argsCount)->getName().data()
          };
      PushCallReference(context->globalContext, &ref);
      Write(code, &CALL, sizeof(CALL));

      x86cmd5byte cmd =
          {
            VEX_VALUES[int(VEX::C4)],
            { X86CMD_MAP_SELECT, rax < r8, X86CMD_X, locations[0] < xmm8 },
            { VMOVQ_XMM_XMM_OPCODES_EXTENSIONS, X86CMD_L, VMOVQ_VVVV, VMOVQ_REG_XMM_W },
            VMOVQ_XMM_REG_OPCODE,
            { REG_TO_ARG(rax), XMM_TO_ARG(locations[0]), REG_REG_MOD }
          };
      Write(code, &cmd, VEX_SIZES[int(VEX::C4)]);
      CleanupValues(context, values, argsCount, code);
      return true;
    }

}