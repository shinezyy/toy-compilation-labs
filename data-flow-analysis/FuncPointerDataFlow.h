//
// Created by diamo on 2017/12/21.
//

#ifndef CLANG_DEVEL_CLION_FUNCPOINTERDATAFLOW_H
#define CLANG_DEVEL_CLION_FUNCPOINTERDATAFLOW_H


#include <llvm/Pass.h>
#include <map>
#include <set>
#include <vector>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>

#include "log.h"


///!TODO TO BE COMPLETED BY YOU FOR ASSIGNMENT 3
using namespace llvm;

class FuncPtrPass : public ModulePass {
public:
    static char ID; // Pass identification, replacement for typeid
    FuncPtrPass() : ModulePass(ID) {}

    typedef std::set<Value*> PossibleFuncPtrSet;
    typedef std::map<Value*, PossibleFuncPtrSet> Env;
    std::map<BasicBlock*, Env> envs;
    Env *_currEnv;
    std::map<Function *, std::map<Instruction *, Env>> argsEnv;  // We mix all args in one context, hope it not to go wrong
    Env returned;  // Record the values each func can return.
    // Record the possible contents of allocated values which can pass through functions implicitly.
    // Updated before callsite.
    // Override each callsite, union different callsite.
    std::map<Function *, std::map<Instruction *, Env>> heapEnvPerFunc;
    // Record the modification of the memory by each function, which should be visible to their callers.
    // We assume each function exit at the ret inst, and update this container using currEnv.
    std::map<Function *, Env> dirtyEnvPerFunc;
    std::map<Instruction *, Value *> allocated;  // Record the value an allocating instruction created.
    std::set<Value *> allocatedValues;  // Record values dyn allocated
    std::set<Value *> dirtyValues; // during a function...
    void markDirty(Value *p) { dirtyValues.insert(p); }
#define currEnv (*_currEnv)

    bool runOnModule(Module &M) override;

    bool iterate(Module &M);

    Env meet(BasicBlock *bb);

    bool dispatchInst(Instruction &inst);

    bool visitPhiNode(PHINode *phiNode);

    bool visitCall(CallInst *callInst);

    bool visitAlloc(AllocaInst *allocaInst);

    bool visitGetElementPtr(GetElementPtrInst* getElementPtrInst);

    bool visitLoad(LoadInst *loadInst);

    bool visitStore(StoreInst *storeInst);

    bool visitReturn(ReturnInst *returnInst);

    bool visitBitcast(BitCastInst *bitCastInst);

    bool visitMemCopy(MemCpyInst *copyInst);

    Value *createAllocValue(Instruction *allloc);

    Env passArgs(Function *func);

    // 下面是一些工具

    bool isFunctionPointer(Type *type);

    PossibleFuncPtrSet wrappedPtrSet(Value *value);

    void checkInit(Value *value);

    bool setUnion(PossibleFuncPtrSet &dst, const PossibleFuncPtrSet &src);

    bool isLLVMBuiltIn(CallInst *callInst);

    void envUnion(Env &dst, const Env &src);

    // For each key in dst, if src has this key, override the content.
    void updateEnv(Env &dst, const Env &src);

    // 输出相关

    void printSet(Value *v);

    void printSet(const PossibleFuncPtrSet &s);

    void printEnv(Env &env);

    void printCalls(Module &M);

    // Note: this function can only be called once per log.
    static const char *nameOf(Value *v) {
        return v->getName().str().c_str();
    }

    raw_ostream& dbg() {
        if (DEBUG_ALL) {
            return llvm::errs();
        }
        else {
            return llvm::nulls();
        }
    }

};


#endif //CLANG_DEVEL_CLION_FUNCPOINTERDATAFLOW_H
