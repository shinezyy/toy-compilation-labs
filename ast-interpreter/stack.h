//
// Created by diamo on 2017/10/12.
//

#ifndef CLANG_DEVEL_CLION_STACK_H
#define CLANG_DEVEL_CLION_STACK_H

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"
#include "log.h"
#include "value.h"

class StackFrame {
    /// StackFrame maps Variable Declaration to Value
    /// Which are either integer or addresses (also represented using an Integer value)
    std::map<clang::Decl *, Value> mVars;
    std::map<clang::Stmt *, Value> mExprs;

    /// The current stmt
    clang::Stmt *mPC;
public:
    StackFrame() : mVars(), mExprs(), mPC() {
    }

//    void bindDecl(clang::Decl *decl, Value &value) {
//        mVars[decl] = value;
//    }

    void bindDecl(clang::Decl *decl, Value value) {
        mVars[decl] = value;
    }

    Value getDeclVal(clang::Decl *decl) {
        assert (mVars.find(decl) != mVars.end());
        return mVars.find(decl)->second;
    }

//    void bindStmt(clang::Stmt *stmt, Value &value) {
//        mExprs[stmt] = value;
//    }

    void bindStmt(clang::Stmt *stmt, Value value) {
        mExprs[stmt] = value;
    }

    Value getStmtVal(clang::Stmt *stmt) {
        if (clang::IntegerLiteral * IL = llvm::dyn_cast<clang::IntegerLiteral>(stmt)) {
            int val = static_cast<int>(IL->getValue().getLimitedValue());
            return Value(val);
        }

        if (mExprs.find(stmt) == mExprs.end()) {
            clang::LangOptions LangOpts;
            LangOpts.CPlusPlus = static_cast<unsigned int>(true);
            clang::PrintingPolicy Policy(LangOpts);
            std::string stmt_str;
            llvm::raw_string_ostream rso(stmt_str);
            stmt->printPretty(rso, nullptr, Policy);

            log(FunctionCall, "No binding value found for var %s\n", rso.str().c_str());
            assert(false);
        }

        return mExprs[stmt];
    }

    void setPC(clang::Stmt *stmt) {
        mPC = stmt;
    }

    clang::Stmt *getPC() {
        return mPC;
    }

    Value returnValue;
};


#endif //CLANG_DEVEL_CLION_STACK_H
