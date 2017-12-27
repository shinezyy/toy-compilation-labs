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
        return ptrSetMap[value];
    }
}

void FuncPtrPass::checkInit(Value *value)
{
    if (!ptrSetMap.count(value)) {
        ptrSetMap[value];
    }
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
           isa<MemSetInst>(callInst);
}

void FuncPtrPass::printSet(Value *v) {
    if (dyn_cast<Function>(v)) {
        llvm::errs() << "{ " << v->getName() << " }\n";
    }
    else {
        std::stringstream ss;
        std::string sep = "";
        ss << "{ ";
        for (auto p : ptrSetMap[v]) {
            ss << sep << p->getName().str();
            sep = ", ";
        }
        ss << " }\n";
        llvm::errs() << ss.str();
    }
}

