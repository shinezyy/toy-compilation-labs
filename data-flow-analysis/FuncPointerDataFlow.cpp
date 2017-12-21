//
// Created by diamo on 2017/12/21.
//


#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Module.h>

#include "FuncPointerDataFlow.h"

char FuncPtrPass::ID = 0;

bool FuncPtrPass::runOnModule(Module &M)
{
    errs() << "Hello: ";
    errs().write_escaped(M.getName()) << '\n';
    M.dump();
    errs() << "------------------------------\n";
    return false;
}

bool FuncPtrPass::iterate(Module &module)
{
    bool updated = false;
    for (auto &function: module.getFunctionList()) {
        for (auto &bb: function) {
            for (auto &inst: bb) {
                updated |= dispatchInst(inst);
            }
        }
    }
    return updated;
}

bool FuncPtrPass::dispatchInst(Instruction &inst)
{
    if (auto casted = dyn_cast<PHINode>(&inst)) {
        return visitPhiNode(casted);
    }
    if (auto casted = dyn_cast<CallInst>(&inst)) {
        return visitCall(casted);
    }
    if (auto casted = dyn_cast<AllocaInst>(&inst)) {
        return visitAlloc(casted);
    }
    if (auto casted = dyn_cast<GetElementPtrInst>(&inst)) {
        return visitGetElementPtr(casted);
    }
    return false;
}

bool FuncPtrPass::visitPhiNode(PHINode *phiNode)
{
    return false;
}

bool FuncPtrPass::visitCall(CallInst *callInst)
{
    return false;
}

bool FuncPtrPass::visitAlloc(AllocaInst *allocaInst)
{
    return false;
}

bool FuncPtrPass::visitGetElementPtr(GetElementPtrInst *getElementPtrInst)
{
    return false;
}
