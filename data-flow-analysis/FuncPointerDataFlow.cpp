//
// Created by diamo on 2017/12/21.
//


#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/CFG.h>

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
    bool updated = true;
    if (DEBUG_ALL) {
        for (unsigned i = 0; i < 80; i++) {
            errs() << '=';
        }
        errs() << '\n';
    }

    Env oldArgsEnv = argsEnv;

    for (auto &function: module.getFunctionList()) {
        if (function.getBasicBlockList().empty()) continue;

        log(DEBUG, FuncVisit, "visit function %s", nameOf(&function));

        // Passing args

        updated = true;
        while (updated) {
            updated = false;

            for (auto &bb: function) {
                log(DEBUG, FuncVisit, "visit bb %s", bb.getName().str().c_str());
                Env in, out;

                if (&bb == &function.getEntryBlock()) {  // pass arguments to entry block.
                    in = argsEnv;
                }
                else {
                    logs(DEBUG, FuncVisit, "meet");
                    in = meet(&bb);
                }

                logs(DEBUG, FuncVisit, "in");
                printEnv(in);

                out = in;

                _currEnv = &out;
                for (auto &inst: bb) {
                    dispatchInst(inst);
                }

                logs(DEBUG, FuncVisit, "out");
                printEnv(out);
                if (out != envs[&bb]) {
                    envs[&bb] = out;
                    logs(DEBUG, FuncVisit, "updated");
                    updated = true;
                }
            }
        }
    }

    // Functions only care args changes.
    return argsEnv != oldArgsEnv;
}

FuncPtrPass::Env FuncPtrPass::meet(BasicBlock *bb) {
    Env in;
    for (auto *p : predecessors(bb)) {
        for (auto pair : envs[p]) {
            log(DEBUG, FuncVisit, "merge %s", nameOf(pair.first));
            setUnion(in[pair.first], pair.second);
        }
    }
    return in;
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
    if (auto bit = dyn_cast<BitCastInst>(&inst)) {
        return visitBitcast(bit);
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
    for (unsigned incoming_index = 0, e = phiNode->getNumIncomingValues(); incoming_index != e; incoming_index++) {
        Value *inVal = phiNode->getIncomingValue(incoming_index);
        if (dyn_cast<Function>(inVal)) {
            setUnion(currEnv[phiNode], wrappedPtrSet(inVal));
        }
        else {
            setUnion(currEnv[phiNode], currEnv[inVal]);
        }
    }
    log(DEBUG, PhiVisit, "length of merged %s: %lu", nameOf(phiNode), currEnv[phiNode].size());
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
    if (auto *func = callInst->getCalledFunction()) {
        possible_func_ptr_set.insert(callInst->getCalledFunction());
        // Handle malloc
        if (func->getName() == "malloc") {
            checkInit(callInst);
            if (currEnv[callInst].empty()) {
                logs(DEBUG, FuncVisit, "malloc");
                Value *p = createAllocValue(callInst);
                checkInit(p);
                currEnv[callInst].insert(p);
                printSet(callInst);
                updated = true;
            }
        }
    } else {
        auto called_value = callInst->getCalledValue();
        possible_func_ptr_set = currEnv[called_value];
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
        if (callInst->getType()->isPointerTy()) {
            auto updated_call_inst = setUnion(currEnv[callInst], currEnv[func]);
            log(DEBUG, FuncVisit, "update call inst: %i", updated_call_inst);
            updated |= updated_call_inst;
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
            PossibleFuncPtrSet &dst = argsEnv[&parameter];
            // 考虑arg是函数的情况,wrap一下
            auto updated_parameters = setUnion(dst, wrappedPtrSet(arg));
            log(DEBUG, FuncVisit, "update parameters: %i", updated_parameters);
            updated |= updated_parameters;
        }
    }
    return updated;
}

Value *FuncPtrPass::createAllocValue(Instruction *alloc) {
    static int count = 0;
    if (allocated.count(alloc)) {
        return allocated[alloc];
    }
    else {
        char name[20];
        sprintf(name, "S%d", ++count);
        Value *v = new AllocaInst(IntegerType::get(alloc->getModule()->getContext(), 32), 10, name);
        allocated[alloc] = v;
        return v;
    }
}

bool FuncPtrPass::visitAlloc(AllocaInst *allocaInst)
{
    log(DEBUG, FuncVisit, "alloc for %s", nameOf(allocaInst));
    if (currEnv.count(allocaInst) == 0) {
        Value *p = createAllocValue(allocaInst);
        currEnv[p];
        currEnv[allocaInst].insert(p);
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
    updated = setUnion(currEnv[getElementPtrInst], currEnv[ptr]) || updated;
    printSet(getElementPtrInst);
    return updated;
}

bool FuncPtrPass::visitLoad(LoadInst *loadInst) {
    bool updated = false;
    Value *src = loadInst->getOperand(0);
    log(DEBUG, FuncVisit, "load from %s", nameOf(src));
    log(DEBUG, FuncVisit, "to %d", loadInst->getValueID());
    for (auto p : currEnv[src]) {
        logs(DEBUG, FuncVisit, "merge");
        updated = setUnion(currEnv[loadInst], currEnv[p]) || updated;
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
    for (auto p : currEnv[dst]) {
        logs(DEBUG, FuncVisit, "merge");
        updated = setUnion(currEnv[p], wrappedPtrSet(src)) || updated;
        printSet(p);
    }

    return updated;
}

bool FuncPtrPass::visitReturn(ReturnInst *returnInst)
{
    auto value = returnInst->getReturnValue();
    if (value == nullptr || !value->getType()->isPointerTy()) {
        return false;
    }

    Function *func = returnInst->getParent()->getParent();
    checkInit(value);
    checkInit(func);
    return setUnion(currEnv[func], wrappedPtrSet(value));
}

bool FuncPtrPass::visitBitcast(BitCastInst *bitCastInst) {
    Value *ope = bitCastInst->getOperand(0);
    log(DEBUG, FuncVisit, "bit cast %s", nameOf(ope));
    return setUnion(currEnv[bitCastInst], currEnv[ope]);
}