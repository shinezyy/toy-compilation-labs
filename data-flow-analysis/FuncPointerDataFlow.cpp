//
// Created by diamo on 2017/12/21.
//


#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IntrinsicInst.h>

#include "FuncPointerDataFlow.h"
#include "log.h"

char FuncPtrPass::ID = 0;

bool FuncPtrPass::runOnModule(Module &M)
{
    errs().write_escaped(M.getName()) << '\n';
    iterate(M);
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
    if (isa<DbgValueInst>(callInst)) {
        return false;
    }
    bool updated = false;
    auto called_value = callInst->getCalledValue();
    PossibleFuncPtrSet possible_func_ptr_set;
    // 让直接调用和间接调用的处理代码一致，所以把它包在一个set里面
    if (called_value == nullptr) {
        possible_func_ptr_set.insert(callInst->getCalledFunction());
    } else {
        possible_func_ptr_set = ptrSetMap[called_value];
    }

    for (auto value: possible_func_ptr_set) {
        auto func = dyn_cast<Function>(value);
        if (!func) {
            log(DEBUG, FuncVisit, "null ptr skipped");
            continue;
        }

        // 把所有函数指针绑定到参数上去
        unsigned arg_index = 0;
        for (auto &parameter: func->args()) {
            if (!isFunctionPointer(parameter.getType())) {
                // 不是函数指针的不管
                arg_index++;
                continue;
            }

            // 形参的possible set检查、初始化
            checkInit(&parameter);

            auto arg = callInst->getOperand(arg_index);
            PossibleFuncPtrSet &dst = ptrSetMap[&parameter];
            // 考虑arg是函数的情况,wrap一下
            updated |= setUnion(dst, wrappedPtrSet(arg));
        }
    }
    return updated;
}

bool FuncPtrPass::visitAlloc(AllocaInst *allocaInst)
{
    return false;
}

bool FuncPtrPass::visitGetElementPtr(GetElementPtrInst *getElementPtrInst)
{
    return false;
}

