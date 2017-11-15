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
#include <llvm/IR/DebugInfoMetadata.h>

#include <llvm/Transforms/Scalar.h>

#include <llvm/IR/Function.h>
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>
#include "llvm/Support/Casting.h"
#include <llvm/IR/CFG.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Bitcode/BitcodeWriterPass.h>

#include <map>
#include <list>
#include <algorithm>

//#if LLVM_VERSION_MAJOR >= 4
#if 1
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>

#else

#include <llvm/Bitcode/ReaderWriter.h>

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

    using PossibleFuncPtrList = std::list<Value*>;
    std::map<llvm::CallInst*, PossibleFuncPtrList> callMap{};
    std::map<llvm::PHINode*, PossibleFuncPtrList> phiMap{};
    std::map<llvm::Function*, PossibleFuncPtrList> functionMap{};
    std::map<llvm::Argument*, PossibleFuncPtrList> argMap{};

    std::map<llvm::CmpInst*, PossibleFuncPtrList> nonNullCmpMap{};
    std::map<llvm::CmpInst*, bool> constCmpMap{};
    std::map<llvm::BranchInst*, PossibleFuncPtrList> nonNullBranchMap{};
    std::map<llvm::BranchInst*, BasicBlock*> impossibleBranch{};

    bool unionPossibleList(PossibleFuncPtrList &dst, PossibleFuncPtrList &src) {
        auto updated = false;
        for (auto it: src) {
            if (std::find(dst.begin(), dst.end(), it) == dst.end()) {
                dst.push_back(it);
//                errs() << "Insert " << *it << " into dst\n";
                updated = true;
            } else {
//                errs() << *it << " already in dst\n";
            }
        }
        return updated;
    }

    bool unionPossibleList(PossibleFuncPtrList &dst, Value *value) {
        auto updated = false;
        if (CallInst *call_inst = dyn_cast<CallInst>(value)) {
            assert(callMap.find(call_inst) != callMap.end());
            updated = unionPossibleList(dst, callMap[call_inst]) || updated;

        } else if (auto phi_node = dyn_cast<PHINode>(value)) {
            assert(phiMap.find(phi_node) != phiMap.end());
            updated = unionPossibleList(dst, phiMap[phi_node]) || updated;

        } else if (auto arg = dyn_cast<Argument>(value)) {
            assert(argMap.find(arg) != argMap.end());
//            errs() << "Unioning possible list for argument\n";
            updated = unionPossibleList(dst, argMap[arg]) || updated;
        } else if (isa<Function>(value)) {
//            errs() << "Add " << value->getName() << " to possible list\n";
            if (std::find(dst.begin(), dst.end(), value) == dst.end()) {
                dst.push_back(value);
                updated = true;
            }
        } else {
            if (std::find(dst.begin(), dst.end(), value) == dst.end()) {
                dst.push_back(value);
                updated = true;
            }
        }
        return updated;
    }

    PossibleFuncPtrList getPossibleList(Value *value, BasicBlock *useSite=nullptr) {
        if (CallInst *call_inst = dyn_cast<CallInst>(value)) {
//            errs() << "get possible list hit in call inst\n";
            assert(callMap.find(call_inst) != callMap.end());
            return callMap[call_inst];

        } else if (auto phi_node = dyn_cast<PHINode>(value)) {
//            errs() << "get possible list hit in phi\n";
            PossibleFuncPtrList possibleFuncPtrList;
            for (unsigned i = 0, e = phi_node->getNumIncomingValues();
                 i != e; i++) {
                auto incoming_value = phi_node->getIncomingValue(i);
                auto incoming_BB = phi_node->getIncomingBlock(i);
                auto br = getPredBranch(incoming_BB);
                if (impossibleBranch.find(br) != impossibleBranch.end()) {
                    if (impossibleBranch[br] == incoming_BB) {
                        continue;
                    }
                }
                if (incoming_BB != killedPred) {
//                    errs() << "Incoming Value: ";
//                    incoming_value->dump();
                    unionPossibleList(possibleFuncPtrList, incoming_value);
                }
                if (!useSite) {
                    continue;
                }
//                errs() << "BB name:" << useSite->getName() << "\n";
                if (BranchInst *br = getPredBranch(useSite)) {
                    if (nonNullBranchMap.find(br) != nonNullBranchMap.end()) {
                        for (auto &it : nonNullBranchMap[br]) {
//                            errs() << "reach 4!\n";
                            if (it == value) {
//                                errs() << "reach 5!\n";
                                auto ptr_it = possibleFuncPtrList.begin();
                                while (ptr_it != possibleFuncPtrList.end()) {
//                                    errs() << "reach 6!\n";
                                    if ((*ptr_it)->getName() == "") {
//                                        errs() << "reach 7!\n";
//                                        errs() << "erase null: " << (*ptr_it)->getName() << "\n";
                                        ptr_it = possibleFuncPtrList.erase(ptr_it);
                                    } else {
                                        ptr_it++;
                                    }
                                }
                            } else {
//                                errs() << "it: " << it->getName() << " | value: "
//                                       << value->getName() << "\n";
                            }
                        }
                    }
                }
            }
            return possibleFuncPtrList;

        } else if (auto arg = dyn_cast<Argument>(value)) {
//            errs() << "get possible list hit in arg\n";
            assert(argMap.find(arg) != argMap.end());
            return argMap[arg];

        } else if (isa<Function>(value)) {
//            errs() << "get possible list hit in func\n";
            return PossibleFuncPtrList(1, value);

        } else {
//            errs() << *value;
            assert("Unexpected value type!\n" && false);
        }
    }

    void initMap(Module &module) {
        // TODO: find all phi of func ptr and function calls
        for (auto &function: module.getFunctionList()) {
            functionMap[&function] = PossibleFuncPtrList();
            for (auto &arg : function.args()) {
                argMap[&arg] = PossibleFuncPtrList();
            }
            for (auto &bb : function) {
                for (auto &inst : bb) {
                    if (auto callInst = dyn_cast<CallInst>(&inst)) {
                        callMap[callInst] = PossibleFuncPtrList();
                    } else if (auto phi = dyn_cast<PHINode>(&inst)) {
                        phiMap[phi] = PossibleFuncPtrList();
                    }
                }
            }
        }
    }

    template <class T>
    void printMap(T t) {
        for (auto &map_it: t) {
            if (isDebugCall(map_it.first)) {
                continue;
            }
            errs() << *(map_it.first) << " ----------------------------- \n";
            for (auto &list_it: map_it.second) {
                if (list_it->getName() == "") {
                    errs() << "null\n";
                } else {
                    errs() << list_it->getName() << "\n";
                }
            }
        }
    }

    void printMaps() {
//        printMap(functionMap);
//        printMap(callMap);
//        printMap(phiMap);
        printMap(argMap);
    }

    int numIter{};

    BasicBlock *killedPred{};

    bool processConstIntCmp(CmpInst *cmp) {
        // process const int
        auto op1 = cmp->getOperand(0);
        auto op2 = cmp->getOperand(1);
        auto op1_int = dyn_cast<llvm::ConstantInt>(op1);
        auto op2_int = dyn_cast<llvm::ConstantInt>(op2);
        if (!(op1_int && op2_int)) return false;

        auto &v1 = op1_int->getValue();
        auto &v2 = op2_int->getValue();

        bool res = false;
        if (cmp->isSigned()) {
            switch (cmp->getPredicate()) {
                case CmpInst::ICMP_EQ: res = v1.eq(v2); break;
                case CmpInst::ICMP_NE: res = v1.ne(v2); break;
                case CmpInst::ICMP_SGT: res = v1.sgt(v2); break;
                case CmpInst::ICMP_SGE: res = v1.sge(v2); break;
                case CmpInst::ICMP_SLT: res = v1.slt(v2); break;
                case CmpInst::ICMP_SLE: res = v1.sle(v2); break;
                case CmpInst::ICMP_UGT: res = v1.ugt(v2); break;
                case CmpInst::ICMP_UGE: res = v1.uge(v2); break;
                case CmpInst::ICMP_ULT: res = v1.ult(v2); break;
                case CmpInst::ICMP_ULE: res = v1.ule(v2); break;
                default:
                    assert("Unexpected comparison!" && false);
            }
        }
        constCmpMap[cmp] = res;
//        errs() << "found const cmp: " << cmp->getName() << "\n";
        return true;
    }

    bool findNullPointer(Value* &op1, Value* &op2) {
        // if one of them is null, set op1 to null

        if (!(isa<ConstantPointerNull>(op1) ||
              isa<ConstantPointerNull>(op2))) {
            return false;
        }
        if (isa<ConstantPointerNull>(op1)) {
            return true;
        }

        // op2 is null
        Value *tmp = op1;
        op1 = op2;
        op2 = tmp;
        return true;
    }

    void processCmp(CmpInst *cmp) {
        if (processConstIntCmp(cmp)) return;

        if (cmp->getPredicate() != CmpInst::ICMP_NE) return;

        // process NULL pointer
        auto op1 = cmp->getOperand(0);
        auto op2 = cmp->getOperand(1);

        if (!findNullPointer(op1, op2)) return;

        if (isa<ConstantPointerNull>(op2)) return;

        PossibleFuncPtrList funcPtrList(1, op2);
        unionPossibleList(nonNullCmpMap[cmp], funcPtrList);
    }

    void processBranch(BranchInst *br) {
        if (!br->isConditional()) return;
        auto *condition = br->getCondition();
        auto *cmp = dyn_cast<CmpInst>(condition);
        if (!cmp) return;

        if (constCmpMap.find(cmp) != constCmpMap.end()) {
            bool cmp_result = constCmpMap[cmp];
//            errs() << *br << " inherent from " << cmp->getName() << "\n";
            if (cmp_result) {
                impossibleBranch[br] = br->getSuccessor(1);
            } else {
                impossibleBranch[br] = br->getSuccessor(0);
            }
//            errs() << impossibleBranch[br]->getName() << "set impossible!\n";
        }

        if (nonNullCmpMap.find(cmp) != nonNullCmpMap.end() &&
                !nonNullCmpMap[cmp].empty()) {
            nonNullBranchMap[br] = nonNullCmpMap[cmp];
            return;
        }
    }

    bool isImpossibleBB(BasicBlock *bb) {
        if (pred_begin(bb) == pred_end(bb)) {
//            errs() << bb->getName() << "Possible 1\n";
            return false;
        }
        for (auto pred_it = pred_begin(bb), pred_e = pred_end(bb);
             pred_it != pred_e; ++pred_it) {
            BasicBlock *pred = *pred_it;
            if (!pred) {
//                errs() << bb->getName() << "Possible 2\n";
                return false;
            }
            auto *br = dyn_cast<BranchInst>(pred->getTerminator());
            if (!(br && impossibleBranch.find(br) != impossibleBranch.end() &&
                    impossibleBranch[br] == bb)) {
//                errs() << bb->getName() << "Possible 3\n";
                return false;
            }
        }
//        errs() << bb->getName() << " is impossible\n";
        return true;
    }

    BranchInst *getPredBranch(BasicBlock *bb) {
        auto *pred = bb->getUniquePredecessor();
        if (!pred) return nullptr;
//        errs() << "reach getPredBranch!\n";

        return dyn_cast<BranchInst>(pred->getTerminator());
    }

    bool iterate(Module &module) {
        // TODO: update all phi and function calls
//        errs() << "Iteration " << numIter << "====================\n";
        bool updated = false;
        for (auto &function: module.getFunctionList()) {
//            errs() << "Iterate Function " << function.getName() << "====================\n";
            for (auto &bb : function) {
                if (isImpossibleBB(&bb)) continue;
//                errs() << "Iterate BB " << bb.getName() << "====================\n";
                if (pred_begin(&bb) == pred_end(&bb)) {
                    for (auto &inst : bb) {
//                            errs() << inst << "||||\n";
                        if (auto callInst = dyn_cast<CallInst>(&inst)) {
                            updated = processCall(callInst) || updated;
                        } else if (auto phi = dyn_cast<PHINode>(&inst)) {
                            updated = processPhi(phi) || updated;
                        } else if (auto cmp = dyn_cast<CmpInst>(&inst)) {
                            processCmp(cmp);
                        } else if (auto br = dyn_cast<BranchInst>(&inst)) {
                            processBranch(br);
                        }
                    }
                }

                for (auto pred_it = pred_begin(&bb), pred_e = pred_end(&bb);
                        pred_it != pred_e; ++pred_it) {
                    BasicBlock *chosen_pred = *pred_it;
                    auto pred_of_chosen_pred = chosen_pred->getUniquePredecessor();

                    for (auto pred_it_2 = pred_begin(&bb), pred_e_2 = pred_end(&bb);
                         pred_it_2 != pred_e_2; ++pred_it_2) {
                        BasicBlock *killed_pred = *pred_it_2;
                        if (killed_pred == chosen_pred && !bb.getUniquePredecessor()) {
//                            errs() << "skipped BB " << killed_pred->getName() << ", because of kill\n";
                            continue;
                        }
                        auto pred_of_killed_pred = killed_pred->getUniquePredecessor();
                        if (pred_of_chosen_pred && pred_of_killed_pred &&
                                pred_of_chosen_pred == pred_of_killed_pred) {
                            killedPred = killed_pred;
                        }

                        for (auto &inst : bb) {
//                            errs() << inst << "||||\n";
                            if (auto callInst = dyn_cast<CallInst>(&inst)) {
                                updated = processCall(callInst) || updated;
                            } else if (auto phi = dyn_cast<PHINode>(&inst)) {
                                updated = processPhi(phi) || updated;
                            } else if (auto cmp = dyn_cast<CmpInst>(&inst)) {
                                processCmp(cmp);
                            } else if (auto br = dyn_cast<BranchInst>(&inst)) {
                                processBranch(br);
                            }
                        }

                        killedPred = nullptr;
                    }
                }
            }
//            updated = processFunction(&function) || updated;
        }
//        errs() << "End Iteration " << numIter++ << "====================\n";
        return updated;
    }

    template <class T>
    bool isDebugCall(T callInst) {
        return isa<DbgValueInst>(callInst);
    }

    StringRef funcNameWrapper(Function *function) {
        if (!function || function->getName() == "") {
            return "NULL";

        } else {
            return function->getName();
        }
    }

    void printCalls(Module &module) {
        unsigned last_line = 0;
        bool first_in_current_line = true;
        bool first_line = true;
        for (auto &function: module.getFunctionList()) {
            for (auto &bb : function) {
                for (auto &inst : bb) {
                    unsigned null_count = 0, pointer_count = 0;
                    Function *unique_func = nullptr;
                    if (auto callInst = dyn_cast<CallInst>(&inst)) {
                        if (isDebugCall(callInst)) {
                            continue;
                        }
                        auto calledValue = callInst->getCalledValue();
                        bool isCallingValue = calledValue != nullptr;
                        std::list<Value *> &&possible_func_list =
                                isCallingValue ? std::move(getPossibleList(calledValue, callInst->getParent())) :
                                PossibleFuncPtrList(1, callInst->getCalledFunction());
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
                        for (auto value : possible_func_list) {
                            auto func = dyn_cast<Function>(value);
//                            assert(func);
                            if (!func|| func->getName() == "") {
//                                errs() << "null!";
                                null_count ++;
                                continue;
                            }
                            unique_func = func;
                            pointer_count++;
                            if (!first_in_current_line) {
                                errs() << ", ";
                            } else {
                                first_in_current_line = false;
                            }
                            errs() << func->getName();
                        }
                        last_line = current_line;
                        if (null_count == 0 && pointer_count == 1 &&
                                ! isa<Function>(callInst->getCalledValue())) {
                            callInst->setCalledFunction(unique_func);
//                            errs()  << "    ---- Replace " << calledValue->getName() << " with "
//                                    << unique_func->getName();
//                            errs() << "*U";
                        }
                    }
                }
            }
        }
    }

    bool processCall(CallInst *callInst) {
        // TODO: for call, add all phied possible list to possible list
        // If parameter is func ptr, add corresponding possible list to arg's possible list

        if (isDebugCall(callInst)) {
            // ignore these calls
            return false;
        }
        bool updated = false;
        auto calledValue = callInst->getCalledValue();

        bool isCallingValue = calledValue != nullptr;

//        errs() << "Call Inst: ";
//        callInst->dump();
        // TODO: function pointer consumption
        std::list<Value *> &&possible_func_list =
                isCallingValue ? std::move(getPossibleList(calledValue, callInst->getParent())) :
                PossibleFuncPtrList(1, callInst->getCalledFunction());

        // TODO: function pointer arg

//        errs() << "CallInst:  " << *callInst << " ----------------------\n";
        for (auto value : possible_func_list) {
//            errs() << "Called value: " << value->getName() << "\n";
            Function *func = dyn_cast<Function>(value);
            if (!func) {
//                errs() << "null skipped!\n";
                continue;
            } else {
//                errs() << "Function: " << func->getName() << "   ==============\n";
//                errs() << *callInst << "\n";
            }
            unsigned i = 0;
            for (auto &arg : func->args()) {
                if (!isFunctionPointer(arg.getType())) {
                    i++;
                    continue;
                }
//                    errs() << "pass possible list for argument\n";
                auto arg_in = callInst->getArgOperand(i);
//                    errs() << i << "th Arg:" << arg.getName() << "<---" << arg_in->getName() << "\n";
                if (isa<Function>(arg_in)) {
                    PossibleFuncPtrList &possibleList = argMap[&arg];
                    if (std::find(possibleList.begin(), possibleList.end(),
                                  arg_in) == possibleList.end()) {
                        possibleList.push_back(arg_in);
                        updated = true;
                    }

                } else {
//                    errs() << "binding argument for " << arg.getName() << "\n";
                    PossibleFuncPtrList possibleFuncPtrList = getPossibleList(arg_in);
                    updated = unionPossibleList(argMap[&arg], possibleFuncPtrList) || updated;
                }
//                PossibleFuncPtrList &possibleList = argMap[&arg];
//                errs() << i << "th Arg: " << arg.getName() << "     --------------\n";
//                for (auto it : possibleList) {
//                    errs() << it->getName() << "||\n";
//                }
                i++;
            }
            processFunction(func);
        }



        // TODO: function pointer return value
        for (auto value : possible_func_list) {
            Function *func = dyn_cast<Function>(value);
            if (!func || func->getName() == "") {
                continue;
            }
//            errs() << func->getName() << "\n";
            updated = unionPossibleList(callMap[callInst], functionMap[func]) || updated;
            functionMap[func].clear();
        }

        return updated;
    }

    bool processPhi(PHINode *phiNode) {
        // TODO: for phi, add all phied possible list to possible list
//        errs() << "phi is called\n";
        if (!isFunctionPointer(phiNode->getType())) {
//            errs() << "Phi Node is not function pointer\n";
            return false;
        }
//        errs() << "Phi: " << *phiNode << " ^^^^^\n";

        bool updated = false;
        PossibleFuncPtrList &possible_list = phiMap[phiNode];

        for (unsigned i = 0, e = phiNode->getNumIncomingValues();
                i != e; i++) {
            auto incoming_value = phiNode->getIncomingValue(i);
            auto incoming_BB = phiNode->getIncomingBlock(i);
            if (incoming_BB == killedPred) {
//                errs() << incoming_BB->getName() << " is killed and skipped\n";
                continue;
            }
            assert(phiMap.find(phiNode) != phiMap.end());

            // add phied values into possible list

            if (!isa<PHINode>(incoming_value) &&
                    std::find(possible_list.begin(), possible_list.end(),
                              incoming_value) == possible_list.end()) {
                possible_list.push_back(incoming_value);
                updated = true;
//                    errs() << "Value" << i << "  ------------------\n";
//                    errs() << *incoming_value << "\n";
            }

            // add possible list of phied values into possible list
            updated = unionPossibleList(possible_list, incoming_value) || updated;

        }
//        if (updated) {
//            errs() << "Something updated for Phi Node\n";
//        } else {
//            errs() << "Nothing updated for Phi Node\n";
//        }
        return updated;
    }

    bool isFunctionPointer(Type *type) {
        if (!type->isPointerTy()) {
            return false;
        }
        auto pointee = type->getPointerElementType();
        return pointee->isFunctionTy();
    }

    bool processFunction(Function *function) {
        // TODO: for functions, if return value is function ptr,
        // add their possible values to functionMap

//        auto returnType = function->getReturnType();
//        if (!isFunctionPointer(returnType)) {
//            return false;
//        }

        bool updated = false;
        assert(functionMap.find(function) != functionMap.end());
//        errs() << "Enter Function: " << function->getName() << "  FFFFFFFFFFFF\n";
        for (auto &bb : *function) {
            for (auto &inst : bb) {
                if (auto returnInst = dyn_cast<ReturnInst>(&inst)) {
                    auto value = returnInst->getReturnValue();
                    // 函数总是后序遍历的，因此这里假设这些value的possible list 已经有了
                    updated = unionPossibleList(functionMap[function], value) || updated;
                } else if (auto callInst = dyn_cast<CallInst>(&inst)) {
                    updated = processCall(callInst) || updated;
                }
            }
        }
//        if (!updated) {
//            errs() << "Nothing updated for Function\n";
//        }
        return updated;
    }

    bool runOnModule(Module &module) override {
        errs().write_escaped(module.getName()) << '\n';

        initMap(module);

//        printMaps();

        while (iterate(module));

//        errs() << "------------------------------\n";

        printCalls(module);

//        printMaps();
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

//        errs() << "------------------------------\n";
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

    std::error_code EC;
    llvm::raw_fd_ostream OS(InputFilename, EC, llvm::sys::fs::F_None);
    WriteBitcodeToFile(&(*M), OS);
    OS.flush();
}

