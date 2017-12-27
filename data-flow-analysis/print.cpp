//
// Created by diamo on 2017/12/27.
//
#include <llvm/IR/Module.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/DebugInfoMetadata.h>


#include "FuncPointerDataFlow.h"

void FuncPtrPass::printCalls(Module &module)
{
    unsigned last_line = 0;
    bool first_in_current_line = true;
    bool first_line = true;
    for (auto &function: module.getFunctionList()) {
        for (auto &bb : function) {
            for (auto &inst : bb) {
                unsigned null_count = 0, pointer_count = 0;
                if (auto callInst = dyn_cast<CallInst>(&inst)) {
                    if (isLLVMBuiltIn(callInst)) {
                        continue;
                    }
                    auto calledValue = callInst->getCalledValue();
                    PossibleFuncPtrSet possible_func_ptr_set = wrappedPtrSet(calledValue);
                    MDNode *metadata = callInst->getMetadata(0);
                    if (!metadata) {
                        errs() << "No meta data found for " << *callInst;
                        continue;
                    }
                    DILocation *debugLocation = dyn_cast<DILocation>(metadata);
                    if (!debugLocation) {
                        errs() << "No debug location found for " << *callInst;
                        continue;
                    }
                    unsigned current_line = debugLocation->getLine();
                    if (last_line != current_line) {
                        first_in_current_line = true;
                        if (!first_line) {
                            errs() << "\n";
                        } else {
                            first_line = false;
                        }
                        errs() << current_line << " : ";
                    }
                    for (auto value : possible_func_ptr_set) {
                        auto func = dyn_cast<Function>(value);
//                            assert(func);
                        if (!func|| func->getName() == "") {
//                                errs() << "null!";
                            null_count ++;
                            continue;
                        }
                        pointer_count++;
                        if (!first_in_current_line) {
                            errs() << ", ";
                        } else {
                            first_in_current_line = false;
                        }
                        errs() << func->getName();
                    }
                    last_line = current_line;
//                    if (null_count == 0 && pointer_count == 1 &&
//                        ! isa<Function>(callInst->getCalledValue())) {
//                        callInst->setCalledFunction(unique_func);
//                    }
                }
            }
        }
    }
}

