//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

#include <llvm/Support/CommandLine.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/ToolOutputFile.h>
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include <llvm/IR/Module.h>
#include <llvm/IR/Use.h>
#include <llvm/IR/User.h>

#include <llvm/Transforms/Scalar.h>

#include <llvm/IR/Function.h>
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>
#include "llvm/Support/Casting.h"

#include <map>
#include <list>

#if LLVM_VERSION_MAJOR >= 4
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>

#else

#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/IR/IntrinsicInst.h>

#endif
using namespace llvm;
#if LLVM_VERSION_MAJOR >= 4
static ManagedStatic<LLVMContext> GlobalContext;
static LLVMContext &getGlobalContext() { return *GlobalContext; }
#endif
/* In LLVM 5.0, when  -O0 passed to clang , the functions generated with clang will
 * have optnone attribute which would lead to some transform passes disabled, like mem2reg.
 */
#if LLVM_VERSION_MAJOR == 5
struct EnableFunctionOptPass: public FunctionPass {
    static char ID;
    EnableFunctionOptPass():FunctionPass(ID){}
    bool runOnFunction(Function & F) override{
        if(F.hasFnAttribute(Attribute::OptimizeNone))
        {
            F.removeFnAttr(Attribute::OptimizeNone);
        }
        return true;
    }
};

char EnableFunctionOptPass::ID=0;
#endif



///!TODO TO BE COMPLETED BY YOU FOR ASSIGNMENT 2
///Updated 11/10/2017 by fargo: make all functions
///processed by mem2reg before this pass.
struct FuncPtrPass : public ModulePass {
    static char ID; // Pass identification, replacement for typeid
    FuncPtrPass() : ModulePass(ID) {}

    using PossibleFuncList = std::list<Value*>;
    std::map<llvm::CallInst*, PossibleFuncList> callMap{};
    std::map<llvm::PHINode*, PossibleFuncList> phiMap{};
    std::map<llvm::Function*, PossibleFuncList> functionMap{};

    void initMap(Module &module) {
        // TODO: find all phi of func ptr and function calls
        for (auto &function: module.getFunctionList()) {
            functionMap[&function] = PossibleFuncList();
            for (auto &bb : function) {
                for (auto &inst : bb) {
                    if (auto callInst = dyn_cast<CallInst>(&inst)) {
                        callMap[callInst] = PossibleFuncList();
                    } else if (auto phi = dyn_cast<PHINode>(&inst)) {
                        phiMap[phi] = PossibleFuncList();
                    }
                }
            }
        }
    }

    template <class T>
    void printMap(T t) {
        for (auto &map_it: t) {
            errs() << *(map_it.first) << "\n";
            for (auto &list_it: map_it.second) {
                errs() << list_it << "\n";
            }
        }
    }

    void printMaps() {
//        printMap(functionMap);
//        printMap(callMap);
        printMap(phiMap);
    }

    bool iterate(Module &module) {
        // TODO: update all phi and function calls
        bool updated = false;
        for (auto &function: module.getFunctionList()) {
            for (auto &bb : function) {
                for (auto &inst : bb) {
                    if (auto callInst = dyn_cast<CallInst>(&inst)) {
                        updated = updated || processCall(callInst);
                    } else if (auto phi = dyn_cast<PHINode>(&inst)) {
                        updated = updated || processPhi(phi);
                    }
                }
            }
            updated = updated || processFunction(&function);
        }
        return updated;
    }

    bool processCall(CallInst *callInst) {
        // TODO: for call, add all phied possible list to possible list
        // If parameter is func ptr, add corresponding possible list to arg's possible list
        bool isCallingValue;
        bool hasFuncPtrArg; // has function pointer arguments
        bool isReturnFuncPtr;

        return false;
    }

    bool processPhi(PHINode *phiNode) {
        // TODO: for phi, add all phied possible list to possible list
        for (unsigned i = 0, e = phiNode->getNumIncomingValues();
                i != e; i++) {
            auto value = phiNode->getIncomingValue(i);
            errs() << "Value" << i << "  ------------------\n";
            errs() << *value << "\n";
        }
        return false;
    }

    bool processFunction(Function *function) {
        // TODO: for functions, if return value is function ptr,
        return false;
    }

    bool runOnModule(Module &module) override {
        errs().write_escaped(module.getName()) << '\n';

        initMap(module);

        printMaps();

        while (iterate(module));

#ifdef NEVER_DEFINED
        for (auto &function: module.getFunctionList()) {
            for (auto &bb : function) {
                for (auto &inst : bb) {
                    errs() << inst.getName() << "||: " << inst.getOpcodeName() << "\n";
                    if (auto callInst = dyn_cast<CallInst>(&inst)) {
                        errs() << "Call Inst: =================================\n";
                        errs() << inst << "\n";
                        if (DbgValueInst *dbgValueInst = dyn_cast<DbgValueInst>(&inst)) {
                            errs() << *dbgValueInst << "\n";
                        } else {
                            errs() << "Not dbg declare\n";
                        }
                        if (Function *calledFunction = callInst->getCalledFunction()) {
//                            errs() << "Called function: ---------------------------";
//                            errs() << *calledFunction << "\n";

                        } else {
                            errs() << "Called function not found\n";
                            errs() << "Called Value: " << *(callInst->getCalledValue()) << "\n";
                        }
                    } else if (auto binOp = dyn_cast<BinaryOperator>(&inst)) {
                        errs() << "Binary Operation: =================================\n";
                        errs() << *binOp << "\n";
                    } else if (auto load = dyn_cast<LoadInst>(&inst)) {
                        errs() << "Load: =================================\n";
                        errs() << *load << "\n";
                    }
                }
            }
        }
#endif

        errs() << "------------------------------\n";
        return false;
    }
};


char FuncPtrPass::ID = 0;
static RegisterPass<FuncPtrPass> X("funcptrpass", "Print function call instruction");

static cl::opt<std::string>
        InputFilename(cl::Positional,
                      cl::desc("<filename>.bc"),
                      cl::init(""));


int main(int argc, char **argv) {
    LLVMContext &Context = getGlobalContext();
    SMDiagnostic Err;
    // Parse the command line to read the Inputfilename
    cl::ParseCommandLineOptions(argc, argv,
                                "FuncPtrPass \n My first LLVM too which does not do much.\n");


    // Load the input module
    std::unique_ptr<Module> M = parseIRFile(InputFilename, Err, Context);
    if (!M) {
        Err.print(argv[0], errs());
        return 1;
    }

    llvm::legacy::PassManager Passes;

    ///Remove functions' optnone attribute in LLVM5.0
#if LLVM_VERSION_MAJOR == 5
    Passes.add(new EnableFunctionOptPass());
#endif
    ///Transform it to SSA
    Passes.add(llvm::createPromoteMemoryToRegisterPass());

    /// Your pass to print Function and Call Instructions
    Passes.add(new FuncPtrPass());
    Passes.run(*M.get());
}

