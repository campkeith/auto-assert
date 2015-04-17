#include "util.h"
#include <llvm/Pass.h>
#include <llvm/Module.h>
#include <llvm/Constants.h>
#include <llvm/BasicBlock.h>
#include <llvm/LLVMContext.h>
#include <llvm/Instructions.h>

using namespace llvm;
using std::string;
typedef Instruction::BinaryOps BinaryOps;

namespace {

static const BinaryOps shift_opcodes[] = {Instruction::Shl, Instruction::LShr, Instruction::AShr, Instruction::BinaryOpsEnd};
static const BinaryOps divrem_opcodes[] = {Instruction::UDiv, Instruction::URem,
                                           Instruction::SDiv, Instruction::SRem, Instruction::BinaryOpsEnd};

static bool opcode_in_set(BinaryOps opcode, const BinaryOps set[])
{
    for (const BinaryOps * elem = set; *elem != Instruction::BinaryOpsEnd; elem++)
    {
        if (opcode == *elem)
        {
            return true;
        }
    }
    return false;
}

struct AutoAssertPass : ModulePass
{
    static char ID;
    LLVMContext & context;
    Function * assert_func;
    BasicBlock::iterator cursor;

    AutoAssertPass() : ModulePass(ID), context(getGlobalContext()) {}

    bool runOnModule(Module & module)
    {
        createAssertPrototype(&module);
        insertAssertions(&module);
        return true;
    }

    void createAssertPrototype(Module * module)
    {
        FunctionType * func_type = FunctionType::get(Type::getVoidTy(context), Type::getInt1Ty(context), false);
        assert_func = cast<Function>(module->getOrInsertFunction("assert", func_type));
    }

    void insertAssertions(Module * module)
    {
        for each(function, *module)
        {
            for each(block, *function)
            {
                for each(inst, *block)
                {
                    cursor = inst;
                    handleInstruction(block, inst);
                }
            }
        }
    }

    void handleInstruction(BasicBlock * block, Instruction * inst)
    {
        BinaryOperator * bin_op = dyn_cast<BinaryOperator>(inst);
        if (bin_op)
        {
            BinaryOps opcode = bin_op->getOpcode();
            if (opcode_in_set(opcode, shift_opcodes))
            {
                assertShiftInBounds(bin_op);
            }
            if (bin_op->hasNoSignedWrap())
            {
                assertNoSignedWrap(bin_op);
            }
            if (opcode_in_set(opcode, divrem_opcodes))
            {
                assertDivRemNonzero(bin_op);
            }
        }
    }

    void assertShiftInBounds(BinaryOperator * inst)
    {
        IntegerType * type = cast<IntegerType>(inst->getType());
        ConstantInt * shift_limit = ConstantInt::get(type, type->getBitWidth());
        createAssertion(new ICmpInst(cursor, CmpInst::ICMP_ULT, inst->getOperand(1), shift_limit));
    }

    void assertNoSignedWrap(BinaryOperator * inst)
    {
        BinaryOps opcode = inst->getOpcode();
        unsigned width = cast<IntegerType>(inst->getType())->getBitWidth();
        unsigned new_width = opcode == Instruction::Add ? width + 1
                           : opcode == Instruction::Sub ? width + 1
                           : opcode == Instruction::Mul ? 2 * width
                           : opcode == Instruction::Shl ? 2 * width - 1
                           : (llvm_unreachable("assertNoSignedWrap: unexpected opcode"), 0);
        IntegerType * new_type = IntegerType::get(context, new_width);
        SExtInst * operands[2] = { new SExtInst(inst->getOperand(0), new_type, "", cursor),
                                   new SExtInst(inst->getOperand(1), new_type, "", cursor) };
        BinaryOperator * new_op = BinaryOperator::Create(opcode, operands[0], operands[1], "", cursor);
        Constant * min_value = ConstantInt::get(new_type, APInt::getSignedMinValue(width));
        Constant * max_value = ConstantInt::get(new_type, APInt::getSignedMaxValue(width));
        createAssertion(new ICmpInst(cursor, CmpInst::ICMP_SGE, new_op, min_value));
        createAssertion(new ICmpInst(cursor, CmpInst::ICMP_SLE, new_op, max_value));
    }

    void assertDivRemNonzero(BinaryOperator * inst)
    {
        Constant * zero = ConstantInt::get(inst->getType(), 0);
        createAssertion(new ICmpInst(cursor, CmpInst::ICMP_NE, inst->getOperand(1), zero));
    }

    void createAssertion(Value * predicate)
    {
        CallInst::Create(assert_func, predicate, "", cursor);
    }
};
char AutoAssertPass::ID;

static RegisterPass<AutoAssertPass> pass("auto-assert", "Add automatically generated assertions");

}
