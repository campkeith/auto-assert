 /* To do
    getelementptr inbounds
    sdiv,srem overflow?
 */

#include "util.h"
#include <llvm/Pass.h>
#include <llvm/Module.h>
#include <llvm/Constants.h>
#include <llvm/BasicBlock.h>
#include <llvm/LLVMContext.h>
#include <llvm/Instructions.h>

using namespace llvm;
using std::string;
namespace {

const unsigned shift_opcodes[] = {Instruction::Shl, Instruction::LShr, Instruction::AShr, 0};
const unsigned divrem_opcodes[] = {Instruction::UDiv, Instruction::URem, Instruction::SDiv, Instruction::SRem, 0};

static bool opcode_in_set(unsigned opcode, const unsigned set[])
{
    for (const unsigned * elem = set; *elem; elem++)
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
    Instruction * cursor;

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
            if (bin_op->hasNoSignedWrap())
            {
                assertNoSignedWrap(bin_op);
            }
            if (opcode_in_set(bin_op->getOpcode(), shift_opcodes))
            {
                assertShiftInBounds(bin_op);
            }
            if (opcode_in_set(bin_op->getOpcode(), divrem_opcodes))
            {
                assertDivNonzero(bin_op);
            }
        }
    }

    void assertNoSignedWrap(BinaryOperator * inst)
    {
        Instruction::BinaryOps opcode = inst->getOpcode();
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
        createAssertion(new ICmpInst(cursor, CmpInst::ICMP_SGE, new_op, ConstantInt::get(new_type, APInt::getSignedMinValue(width))));
        createAssertion(new ICmpInst(cursor, CmpInst::ICMP_SLE, new_op, ConstantInt::get(new_type, APInt::getSignedMaxValue(width))));
    }

    void assertShiftInBounds(BinaryOperator * inst)
    {
        IntegerType * type = cast<IntegerType>(inst->getType());
        ConstantInt * shift_limit = ConstantInt::get(type, type->getBitWidth());
        createAssertion(new ICmpInst(cursor, CmpInst::ICMP_ULT, inst->getOperand(1), shift_limit));
    }

    void assertDivNonzero(BinaryOperator * inst)
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
