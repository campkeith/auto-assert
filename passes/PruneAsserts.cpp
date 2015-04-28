#include "util.h"
#include <llvm/Pass.h>
#include <llvm/Function.h>
#include <llvm/Constants.h>
#include <llvm/BasicBlock.h>
#include <llvm/Instructions.h>

using namespace llvm;
using std::vector;
namespace {

struct PruneAssertsPass : BasicBlockPass
{
    static char ID;
    PruneAssertsPass() : BasicBlockPass(ID) {}

    bool runOnBasicBlock(BasicBlock & block)
    {
        vector<CallInst *> const_asserts;
        for each(inst, block)
        {
            CallInst * call = dyn_cast<CallInst>(inst);
            if (call && call->getCalledFunction()->getName() == "assert")
            {
                ConstantInt * const_arg = dyn_cast<ConstantInt>(call->getArgOperand(0));
                if (const_arg)
                {
                    assert(const_arg->isOne());
                    const_asserts.push_back(call);
                }
            }
        }
        for each(assert_call, const_asserts)
        {
            (*assert_call)->eraseFromParent();
        }
        return !const_asserts.empty();
    }
};
char PruneAssertsPass::ID;

static RegisterPass<PruneAssertsPass> pass("prune-asserts", "Prune assertions with a constant predicate");

}
