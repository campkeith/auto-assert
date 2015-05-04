#include "util.h"
#include <llvm/Pass.h>
#include <llvm/Function.h>
#include <llvm/Constants.h>
#include <llvm/BasicBlock.h>
#include <llvm/Instructions.h>
#include <set>

using namespace llvm;
using std::vector;
using std::set;
namespace {

struct PruneAssertsPass : FunctionPass
{
    static char ID;
    PruneAssertsPass() : FunctionPass(ID) {}

    bool runOnFunction(Function & function)
    {
        set<Value *> assert_preds;
        vector<CallInst *> delete_list;
        for each(block, function)
        {
            for each(inst, *block)
            {
                CallInst * call = dyn_cast<CallInst>(inst);
                if (call && call->getCalledFunction()->getName() == "assert")
                {
                    pruneAssert(call, assert_preds, delete_list);
                }
            }
        }
        for each(assert_call, delete_list)
        {
            (*assert_call)->eraseFromParent();
        }
        return !delete_list.empty();
    }

    void pruneAssert(CallInst * assert_call, set<Value *> & assert_preds, vector<CallInst *> & delete_list)
    {
        Value * predicate = assert_call->getArgOperand(0);
        ConstantInt * const_pred = dyn_cast<ConstantInt>(predicate);
        if (const_pred)
        {
            assert(const_pred->isOne());
            delete_list.push_back(assert_call);
        }
        else
        {
            bool inserted = assert_preds.insert(predicate).second;
            if (!inserted)
            {
                delete_list.push_back(assert_call);
            }
        }
    }
};
char PruneAssertsPass::ID;

static RegisterPass<PruneAssertsPass> pass("prune-asserts",
        "Prune redundant assertions and assertions with a constant predicate");

}
