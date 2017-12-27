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
    Env argsEnv;  // We mix all args in one context, hope it not to go wrong
    Env returned;  // Record the values each func can return.
    std::map<Instruction *, Value *> allocated;  // Record the value an allocating instruction created.
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

    Value *createAllocValue(Instruction *allloc);

    // 下面是一些工具

    bool isFunctionPointer(Type *type);

    PossibleFuncPtrSet wrappedPtrSet(Value *value);

    void checkInit(Value *value);

    bool setUnion(PossibleFuncPtrSet &dst, const PossibleFuncPtrSet &src);

    bool isLLVMBuiltIn(CallInst *callInst);

    // 输出相关

    void printSet(Value *v);

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
