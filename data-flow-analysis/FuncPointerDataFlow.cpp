//
// Created by diamo on 2017/12/21.
//


#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/LLVMContext.h>

#include "FuncPointerDataFlow.h"
#include "log.h"

char FuncPtrPass::ID = 0;

bool FuncPtrPass::runOnModule(Module &M)
{
    errs().write_escaped(M.getName()) << '\n';

    while(iterate(M));

    printCalls(M);
    return false;
}

bool FuncPtrPass::iterate(Module &module)
{
    bool updated = false;
    if (DEBUG_ALL) {
        for (unsigned i = 0; i < 80; i++) {
            errs() << '=';
        }
        errs() << '\n';
    }
    for (auto &function: module.getFunctionList()) {
        if (function.getBasicBlockList().size() == 0) continue;
        log(DEBUG, FuncVisit, "visit function %s", nameOf(&function));
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
    if (auto load = dyn_cast<LoadInst>(&inst)) {
        return visitLoad(load);
    }
    if (auto store = dyn_cast<StoreInst>(&inst)) {
        return visitStore(store);
    }
    if (auto casted = dyn_cast<ReturnInst>(&inst)) {
        return visitReturn(casted);
    }
    return false;
}

bool FuncPtrPass::visitPhiNode(PHINode *phiNode)
{
    if (!isFunctionPointer(phiNode->getType())) {
        return false;
    }
    auto updated = false;
    checkInit(phiNode);
    for (unsigned incoming_index = 0, e = phiNode->getNumIncomingValues();
            incoming_index != e; incoming_index++) {
        updated |= setUnion(
                ptrSetMap[phiNode],
                wrappedPtrSet(phiNode->getIncomingValue(incoming_index)));
    }
    log(DEBUG, PhiVisit, "length of merged set: %lu", ptrSetMap[phiNode].size());
    return updated;
}

bool FuncPtrPass::visitCall(CallInst *callInst)
{
    if (isLLVMBuiltIn(callInst)) {
        return false;
    }
    checkInit(callInst);
    bool updated = false;
    PossibleFuncPtrSet possible_func_ptr_set;
    // 让直接调用和间接调用的处理代码一致，所以把它包在一个set里面
    if (auto func = callInst->getCalledFunction()) {
        possible_func_ptr_set.insert(func);
    } else {
        auto called_value = callInst->getCalledValue();
        possible_func_ptr_set = ptrSetMap[called_value];
    }
    log(DEBUG, FuncVisit, "Size of func ptr set: %lu", possible_func_ptr_set.size());

    for (auto value: possible_func_ptr_set) {
        auto func = dyn_cast<Function>(value);
        if (!func) {
            logs(DEBUG, FuncVisit, "null ptr skipped");
            continue;
        }
        checkInit(func);
        // 把绑定到函数（返回值）上的指针集合并入call Inst
        auto updated_call_inst = setUnion(ptrSetMap[callInst], ptrSetMap[func]);
        log(DEBUG, FuncVisit, "update call inst: %i", updated_call_inst);
        updated |= updated_call_inst;

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
            auto updated_parameters = setUnion(dst, wrappedPtrSet(arg));
            log(DEBUG, FuncVisit, "update parameters: %i", updated_parameters);
            updated |= updated_parameters;
        }
    }
    return updated;
}

Value *FuncPtrPass::createAllocValue(AllocaInst *alloc) {
    static int count = 0;
    char name[20];
    sprintf(name, "S%d", ++count);
    return new AllocaInst(IntegerType::get(alloc->getModule()->getContext(), 32), 10, name);
}

bool FuncPtrPass::visitAlloc(AllocaInst *allocaInst)
{
    log(DEBUG, FuncVisit, "alloc for %s", nameOf(allocaInst));
    if (ptrSetMap.count(allocaInst) == 0) {
        Value *p = createAllocValue(allocaInst);
        ptrSetMap[p];
        ptrSetMap[allocaInst].insert(p);
        printSet(allocaInst);
        return true;
    }
    else {
        return false;
    }
}

bool FuncPtrPass::visitGetElementPtr(GetElementPtrInst *getElementPtrInst)
{
    // Assume field insensitive
    bool updated = false;
    Value *ptr = getElementPtrInst->getOperand(0);
    log(DEBUG, FuncVisit, "get elem of %s", nameOf(ptr));
    log(DEBUG, FuncVisit, "to %s", nameOf(getElementPtrInst));
    updated = setUnion(ptrSetMap[getElementPtrInst], ptrSetMap[ptr]) || updated;
    printSet(getElementPtrInst);
    return updated;
}

bool FuncPtrPass::visitLoad(LoadInst *loadInst) {
    bool updated = false;
    Value *src = loadInst->getOperand(0);
    log(DEBUG, FuncVisit, "load from %s", nameOf(src));
    log(DEBUG, FuncVisit, "to %d", loadInst->getValueID());
    for (auto p : ptrSetMap[src]) {
        log(DEBUG, FuncVisit, "merge");
        updated = setUnion(ptrSetMap[loadInst], ptrSetMap[p]) || updated;
        printSet(loadInst);
    }
    return updated;
}

bool FuncPtrPass::visitStore(StoreInst *storeInst) {
    bool updated = false;
    Value *src = storeInst->getOperand(0);
    log(DEBUG, FuncVisit, "store src: %s", nameOf(src));
    printSet(src);
    Value *dst = storeInst->getOperand(1);
    log(DEBUG, FuncVisit, "store dst: %s", nameOf(dst));
    printSet(dst);
    for (auto p : ptrSetMap[dst]) {
        log(DEBUG, FuncVisit, "merge");
        updated = setUnion(ptrSetMap[p], wrappedPtrSet(src)) || updated;
        printSet(p);
    }

    return updated;
}

bool FuncPtrPass::visitReturn(ReturnInst *returnInst)
{
    auto value = returnInst->getReturnValue();
    Function *func = returnInst->getParent()->getParent();
    checkInit(value);
    checkInit(func);
    return setUnion(ptrSetMap[func], wrappedPtrSet(value));
}
