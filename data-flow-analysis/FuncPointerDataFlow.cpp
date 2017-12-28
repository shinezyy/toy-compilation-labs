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

    auto oldArgsEnv = argsEnv;
    auto oldReturned = returned;
    auto oldHeap = heapEnvPerFunc;
    auto oldDirty = dirtyEnvPerFunc;

    for (auto &function: module.getFunctionList()) {
        if (function.getBasicBlockList().empty()) continue;

        dbg() << "visit function " << function.getName()
              << "------------------------------------------\n";

        // Passing args

        updated = true;
        while (updated) {
            updated = false;

            for (auto &bb: function) {
                log(DEBUG, FuncVisit, "visit bb %s", bb.getName().str().c_str());
                Env in, out;

                if (&bb == &function.getEntryBlock()) {  // pass arguments to entry block.
                    in = passArgs(&function);
                }
                else {
                    logs(DEBUG, FuncVisit, "meet");
                    in = meet(&bb);
                }

                for (const auto &it : heapEnvPerFunc[&function]) {
                    envUnion(in, it.second);
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

    logs(DEBUG, FuncVisit, "Returns");
    printEnv(returned);

    // Functions only care args and return values changes.
    return argsEnv != oldArgsEnv || returned != oldReturned || heapEnvPerFunc != oldHeap || dirtyEnvPerFunc != oldDirty;
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
        log(DEBUG, PhiVisit, "Encountered non-func-pointer phi node: %s",
            nameOf(phiNode));
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
        if (auto copyInst = dyn_cast<MemCpyInst>(callInst)) {
            return visitMemCopy(copyInst);
        } else {
            return false;
        }
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


    // Reset all funcs' dirty env provided by this callsite,
    // as this call might not be some func's callsite anymore.
    // for (auto &heapPerCall : heapEnvPerFunc) {
    //     heapPerCall.second[callInst].clear();
    // }

    Env dirtyEnv;
    for (auto value: possible_func_ptr_set) {
        auto func = dyn_cast<Function>(value);
        if (!func) {
            logs(DEBUG, FuncVisit, "null ptr skipped");
            continue;
        }
        checkInit(func);

        // 把绑定到函数（返回值）上的指针集合并入call Inst
        if (callInst->getType()->isPointerTy()) {
            auto updated_call_inst = setUnion(currEnv[callInst], returned[func]);
            log(DEBUG, FuncVisit, "update call inst: %i", updated_call_inst);
            updated |= updated_call_inst;
        }


        // 把所有函数指针绑定到参数上去
        unsigned arg_index = 0;
        for (auto &parameter: func->args()) {
            if (!parameter.getType()->isPointerTy() ) {
                // 不是函数指针的不管
                arg_index++;
                continue;
            }

            // 形参的possible set检查、初始化
            checkInit(&parameter);

            auto arg = callInst->getOperand(arg_index);
            dbg() << "pass ";
            printSet(wrappedPtrSet(arg));
            dbg() << " to " << func->getName() << "'s " << parameter.getName() << "\n";
            argsEnv[func][callInst][&parameter] = wrappedPtrSet(arg);
            arg_index++;
        }

        // Pass current env as callee's heap env.
        auto &myOldProvide = heapEnvPerFunc[func][callInst];
        if (myOldProvide != currEnv) {
            myOldProvide = currEnv;
        }
        else {
            // Fetch dirty data from this called function. Union all candidates
            envUnion(dirtyEnv, dirtyEnvPerFunc[func]);
            dbg() << "dirty env of " << func->getName() << ":\n";
            printEnv(dirtyEnvPerFunc[func]);
            dbg() << "end\n";
        }
    }


    updateEnv(currEnv, dirtyEnv);

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
        currEnv[p] = wrappedPtrSet(src);
        printSet(p);
    }

    return updated;
}

bool FuncPtrPass::visitReturn(ReturnInst *returnInst)
{
    Function *func = returnInst->getParent()->getParent();

    Env heap;
    for (const auto &it : heapEnvPerFunc[func]) {
        envUnion(heap, it.second);
    }
    dirtyEnvPerFunc[func].clear();
    for (auto &kv : heap) {
        if (currEnv[kv.first] != kv.second) {
            dbg() << "func " << func->getName() << " dirty value: " << kv.first->getName() << "\n";
            dirtyEnvPerFunc[func][kv.first] = currEnv[kv.first];
        }
    }
    // Update dirty env.
    // if (dirtyEnvPerFunc[func] != currEnv) {
    //     dbg() << "Previous dirty env of " << func->getName() << ":\n";
    //     printEnv(dirtyEnvPerFunc[func]);

    //     dirtyEnvPerFunc[func] = currEnv;

    //     dbg() << "Update to dirty env:\n";
    //     printEnv(currEnv);
    // }

    // Update return value set.
    auto value = returnInst->getReturnValue();
    if (value == nullptr || !value->getType()->isPointerTy()) {
        return false;
    }

    log(DEBUG, FuncVisit, "return %s", nameOf(value));
    printSet(value);
    return setUnion(returned[func], wrappedPtrSet(value));
}

bool FuncPtrPass::visitBitcast(BitCastInst *bitCastInst) {
    Value *ope = bitCastInst->getOperand(0);
    log(DEBUG, FuncVisit, "bit cast %s", nameOf(ope));
    return setUnion(currEnv[bitCastInst], currEnv[ope]);
}

FuncPtrPass::Env FuncPtrPass::passArgs(Function *func) {
    Env in;
    auto &argsPerCallSite = argsEnv[func];
    for (auto &it : argsPerCallSite) {
        for (auto &arg : func->args()) {
            if (arg.getType()->isPointerTy()) {
                setUnion(in[&arg], it.second[&arg]);
            }
        }
    }
    return in;
}

void FuncPtrPass::envUnion(FuncPtrPass::Env &dst, const FuncPtrPass::Env &src) {
    for (auto &pair : src) {
        setUnion(dst[pair.first], pair.second);
    }
}

void FuncPtrPass::updateEnv(FuncPtrPass::Env &dst, const FuncPtrPass::Env &src) {
    for (auto &pair : dst) {
        if (!src.count(pair.first)) {
            continue;
        }
        const auto &set = *src.find(pair.first);
        if (set.second != pair.second) {
            dbg() << "update value " << pair.first->getName() << ": ";
            dbg() << "from ";
            printSet(dst[pair.first]);
            dst[pair.first] = set.second;
            dbg() << " to ";
            printSet(dst[pair.first]);
            dbg() << "\n";
        }
    }
}

bool FuncPtrPass::visitMemCopy(MemCpyInst *copyInst)
{
    logs(DEBUG, LdStVisit, "Visit MemCopy!");
    auto src = copyInst->getSource();
    auto dst = copyInst->getDest();
    for (auto d: currEnv[dst]) {
        currEnv[d].clear();
        for (auto s: currEnv[src]) {
            setUnion(currEnv[d], wrappedPtrSet(s));
        }
    }
    return false;
}

