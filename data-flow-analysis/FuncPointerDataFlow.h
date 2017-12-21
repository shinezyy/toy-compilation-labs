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


///!TODO TO BE COMPLETED BY YOU FOR ASSIGNMENT 3
using namespace llvm;

class FuncPtrPass : public ModulePass {
public:
    static char ID; // Pass identification, replacement for typeid
    FuncPtrPass() : ModulePass(ID) {}

    typedef std::set<Value*> PossibleFuncPtrSet;

    bool runOnModule(Module &M) override;

    bool iterate(Module &M);

    bool dispatchInst(Instruction &inst);

    bool visitPhiNode(PHINode *phiNode);

    bool visitCall(CallInst *callInst);

    bool visitAlloc(AllocaInst *allocaInst);

    bool visitGetElementPtr(GetElementPtrInst* getElementPtrInst);
};


#endif //CLANG_DEVEL_CLION_FUNCPOINTERDATAFLOW_H
