//
// Created by diamo on 2017/12/27.
//
#include <sstream>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/Support/raw_ostream.h>
#include "FuncPointerDataFlow.h"
#include "log.h"

bool FuncPtrPass::isFunctionPointer(Type *type)
{
    if (!type->isPointerTy()) {
        return false;
    }
    auto pointee = type->getPointerElementType();
    return pointee->isFunctionTy();
}

FuncPtrPass::PossibleFuncPtrSet FuncPtrPass::wrappedPtrSet(Value *value)
{
    if (isa<Function>(value)) {
        PossibleFuncPtrSet possibleFuncPtrSet;
        possibleFuncPtrSet.insert(value);
        return possibleFuncPtrSet;
    } else {
        return currEnv[value];
    }
}

void FuncPtrPass::checkInit(Value *value)
{
    return;
}

bool FuncPtrPass::setUnion(FuncPtrPass::PossibleFuncPtrSet &dst,
                           const FuncPtrPass::PossibleFuncPtrSet &src)
{
    bool updated = false;
    for (const auto it: src) {
        bool inserted;
        std::tie(std::ignore, inserted) = dst.insert(it);
        updated |= inserted;
    }
    return updated;
}

bool FuncPtrPass::isLLVMBuiltIn(CallInst *callInst)
{
    return isa<DbgValueInst>(callInst) ||
           isa<DbgDeclareInst>(callInst) ||
           isa<MemSetInst>(callInst) ||
           isa<MemCpyInst>(callInst);
}

void FuncPtrPass::printSet(Value *v) {
    if (DEBUG_ALL && PtrSet && dyn_cast<Function>(v)) {
        llvm::errs() << "{ " << v->getName() << " }\n";
    }
    else {
        std::stringstream ss;
        std::string sep = "";
        ss << "{ ";
        for (auto p : currEnv[v]) {
            ss << sep << p->getName().str();
            sep = ", ";
        }
        ss << " }\n";
        if (DEBUG_ALL && PtrSet) {
            llvm::errs() << ss.str();
        }
    }
}

void FuncPtrPass::printEnv(FuncPtrPass::Env &env) {
    if (!(EnvDebug && DEBUG_ALL)) return;
    for (auto &pair : env) {
        Value *key = pair.first;
        PossibleFuncPtrSet &set = pair.second;
        if (key->getName() == "") {
            llvm::errs() << key->getValueID() << ": ";
        }
        else {
            llvm::errs() << key->getName() << ": ";
        }

        std::stringstream ss;
        std::string sep;
        ss << "{ ";
        for (auto p : set) {
            ss << sep << p->getName().str();
            sep = ", ";
        }
        ss << " }\n";
        llvm::errs() << ss.str();
    }
}


void FuncPtrPass::printSet(const FuncPtrPass::PossibleFuncPtrSet &s) {
    std::stringstream ss;
    std::string sep = "";
    ss << "{ ";
    for (auto p : s) {
        ss << sep << p->getName().str();
        sep = ", ";    }
    ss << " }";
    if (DEBUG_ALL && PtrSet) {
        llvm::errs() << ss.str();
    }
}
