#include "util.h"
#include <llvm/Pass.h>
#include <llvm/Module.h>
#include <llvm/Operator.h>
#include <llvm/Constants.h>
#include <llvm/BasicBlock.h>
#include <llvm/LLVMContext.h>
#include <llvm/Instructions.h>

using namespace llvm;
using std::string;
typedef User::op_iterator op_iterator;
typedef Instruction::BinaryOps BinaryOps;

namespace {

static const BinaryOps shift_opcodes[] = {Instruction::Shl, Instruction::LShr, Instruction::AShr, Instruction::BinaryOpsEnd};
static const BinaryOps divrem_opcodes[] = {Instruction::UDiv, Instruction::URem,
                                           Instruction::SDiv, Instruction::SRem, Instruction::BinaryOpsEnd};
static const BinaryOps sdivrem_opcodes[] = {Instruction::SDiv, Instruction::SRem, Instruction::BinaryOpsEnd};

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
        createAssertFunctionPrototype(&module);
        createAssertions(&module);
        return true;
    }

    void createAssertFunctionPrototype(Module * module)
    {
        FunctionType * func_type = FunctionType::get(Type::getVoidTy(context), Type::getInt1Ty(context), false);
        assert_func = cast<Function>(module->getOrInsertFunction("assert", func_type));
    }

    void createAssertions(Module * module)
    {
        for each(function, *module)
        {
            for each(block, *function)
            {
                for each(inst, *block)
                {
                    cursor = inst;
                    createAssertionsForInstruction(inst);
                }
            }
        }
    }

    void createAssertionsForInstruction(Instruction * inst)
    {
        BinaryOperator * bin_op = dyn_cast<BinaryOperator>(inst);
        GetElementPtrInst * gep = dyn_cast<GetElementPtrInst>(inst);
        if (bin_op)
        {
            BinaryOps opcode = bin_op->getOpcode();
            if (opcode_in_set(opcode, shift_opcodes))
            {
                assertShiftInBounds(bin_op);
            }
            if (isa<OverflowingBinaryOperator>(bin_op) && bin_op->hasNoSignedWrap())
            {
                assertNoSignedWrap(bin_op);
            }
            if (opcode_in_set(opcode, divrem_opcodes))
            {
                assertNoDivRemByZero(bin_op);
            }
            if (opcode_in_set(opcode, sdivrem_opcodes))
            {
                assertNoSDivRemOverflow(bin_op);
            }
        }
        else if (gep && gep->isInBounds())
        {
            assertGetElementPtrInBounds(gep);
        }
    }

    void assertShiftInBounds(BinaryOperator * shift)
    {
        IntegerType * type = cast<IntegerType>(shift->getType());
        ConstantInt * shift_limit = ConstantInt::get(type, type->getBitWidth());
        createAssertion(new ICmpInst(cursor, CmpInst::ICMP_ULT, shift->getOperand(1), shift_limit));
    }

    void assertNoSignedWrap(BinaryOperator * arith)
    {
        BinaryOps opcode = arith->getOpcode();
        unsigned width = cast<IntegerType>(arith->getType())->getBitWidth();
        unsigned new_width = opcode == Instruction::Add ? width + 1
                           : opcode == Instruction::Sub ? width + 1
                           : opcode == Instruction::Mul ? 2 * width
                           : opcode == Instruction::Shl ? 2 * width - 1
                           : (llvm_unreachable("assertNoSignedWrap: unexpected opcode"), 0);
        IntegerType * new_type = IntegerType::get(context, new_width);
        SExtInst * operands[2] = { new SExtInst(arith->getOperand(0), new_type, "", cursor),
                                   new SExtInst(arith->getOperand(1), new_type, "", cursor) };
        BinaryOperator * new_op = BinaryOperator::Create(opcode, operands[0], operands[1], "", cursor);
        Constant * min_value = ConstantInt::get(new_type, APInt::getSignedMinValue(width).sext(new_width));
        Constant * max_value = ConstantInt::get(new_type, APInt::getSignedMaxValue(width).sext(new_width));
        createAssertion(new ICmpInst(cursor, CmpInst::ICMP_SGE, new_op, min_value));
        createAssertion(new ICmpInst(cursor, CmpInst::ICMP_SLE, new_op, max_value));
    }

    void assertNoDivRemByZero(BinaryOperator * divrem)
    {
        Constant * zero = ConstantInt::get(divrem->getType(), 0);
        createAssertion(new ICmpInst(cursor, CmpInst::ICMP_NE, divrem->getOperand(1), zero));
    }

    void assertNoSDivRemOverflow(BinaryOperator * sdivrem)
    {
        IntegerType * type = cast<IntegerType>(sdivrem->getType());
        unsigned width = type->getBitWidth();
        Constant * min_value = ConstantInt::get(sdivrem->getType(), APInt::getSignedMinValue(width));
        Constant * minus_one = ConstantInt::getSigned(type, -1);
        ICmpInst * dividend_pred = new ICmpInst(cursor, CmpInst::ICMP_NE, sdivrem->getOperand(0), min_value);
        ICmpInst *  divisor_pred = new ICmpInst(cursor, CmpInst::ICMP_NE, sdivrem->getOperand(1), minus_one);
        createAssertion(BinaryOperator::Create(Instruction::Or, dividend_pred, divisor_pred, "", cursor));
    }

    void assertGetElementPtrInBounds(GetElementPtrInst * gep)
    {
        Value * base_pointer = gep->getPointerOperand();
        if (isa<GlobalVariable>(base_pointer) || isa<AllocaInst>(base_pointer))
        {
            Value * first_index = *gep->idx_begin();
            Constant * zero = ConstantInt::getSigned(first_index->getType(), 0);
            createAssertion(new ICmpInst(cursor, CmpInst::ICMP_EQ, first_index, zero));
        }
        SequentialType * type = gep->getPointerOperandType();
        for (op_iterator operand = gep->idx_begin() + 1; operand != gep->idx_end(); operand++)
        {
            type = cast<SequentialType>(type->getElementType());
            unsigned size = cast<ArrayType>(type)->getNumElements();
            Constant * zero = ConstantInt::getSigned((*operand)->getType(), 0);
            Constant * index_limit = ConstantInt::getSigned((*operand)->getType(), size);
            createAssertion(new ICmpInst(cursor, CmpInst::ICMP_SGE, *operand, zero));
            createAssertion(new ICmpInst(cursor, CmpInst::ICMP_SLT, *operand, index_limit));
        }
    }

    void createAssertion(Value * predicate)
    {
        CallInst::Create(assert_func, predicate, "", cursor);
    }
};
char AutoAssertPass::ID;

static RegisterPass<AutoAssertPass> pass("auto-assert", "Add automatically generated assertions");

}
