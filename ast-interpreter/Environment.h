//==--- tools/clang-check/ClangInterpreter.cpp - Clang Interpreter tool --------------===//
//===----------------------------------------------------------------------===//
#include <cstdio>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

#include "log.h"
#include "value.h"
#include "stack.h"
#include "heap.h"

using namespace clang;

Value compute(Value &l, Value &r, std::function< int(int, int) > bop_int,
              std::function< unsigned long(unsigned long, unsigned long) > bop_long);


class Environment {
public:
    std::deque<StackFrame> mStack;

private:

    Heap heap;

    FunctionDecl *mFree;                /// Declartions to the built-in functions
    FunctionDecl *mMalloc;
    FunctionDecl *mInput;
    FunctionDecl *mOutput;

    FunctionDecl *mEntry;

public:
    /// Get the declartions to the built-in functions
    Environment() : mStack(), mFree(nullptr), mMalloc(nullptr),
                    mInput(nullptr), mOutput(nullptr), mEntry(nullptr) {
    }


    /// Initialize the Environment
    void init(TranslationUnitDecl *unit) {
        for (TranslationUnitDecl::decl_iterator i = unit->decls_begin(),
                     e = unit->decls_end(); i != e; ++i) {
            if (FunctionDecl *fdecl = dyn_cast<FunctionDecl>(*i)) {
                if (fdecl->getName().equals("FREE")) mFree = fdecl;
                else if (fdecl->getName().equals("MALLOC")) mMalloc = fdecl;
                else if (fdecl->getName().equals("GET")) mInput = fdecl;
                else if (fdecl->getName().equals("PRINT")) mOutput = fdecl;
                else if (fdecl->getName().equals("main")) mEntry = fdecl;
            }
        }
        mStack.push_back(StackFrame());
    }

    FunctionDecl *getEntry() {
        return mEntry;
    }

    void DispatchComputation(BinaryOperator *binaryOperator, Value &x, Value &y);


    /// !TODO Support comparison operation
    void binOp(BinaryOperator *bop);

    Value deRef(Value &value);

    void unaryOp(UnaryOperator *unaryOperator) {
        logs(PointerVisit, "Visiting Unary Operator\n");
        Expr *subExpr = unaryOperator->getSubExpr();

        if (unaryOperator->getOpcode() == UO_Deref) {
            Value value = mStack.front().getStmtVal(subExpr);
            logp(PointerVisit, value.address);
            log_var(PointerVisit, value.pointerLevel);
            mStack.front().bindStmt(unaryOperator, deRef(value));
        }
        logs(PointerVisit, "Visited Unary Operator\n");
    }

    void decl(DeclStmt *declstmt);

    void declRef(DeclRefExpr *declRefexpr);

    void cast(CastExpr *castExpr);

    /// !TODO Support Function Call
    FunctionDecl * call(CallExpr *callExpr);

    void parmDecl(ParmVarDecl *parmVarDecl) {
        logs(FunctionCall, "Visting Param Decl Stmt\n");
        // TODO: iterate through the param list, and set decl values
        assert(false);
    }

    bool getCondition(Expr *expr) {
        Value value = mStack.front().getStmtVal(expr);
        assert(value.typ == Int);
        return static_cast<bool>(value.intValue);
    }

    void setReturnVal(ReturnStmt *returnStmt) {
        mStack.front().returnValue =
                mStack.front().getStmtVal(returnStmt->getRetValue());
    }

    void postCall(CallExpr *callexpr, Value returnVal) {
        // DONE: bind return value with stmt
        mStack.front().bindStmt(callexpr, returnVal);
    }

    void implicitCast(ImplicitCastExpr *implicitCastExpr);

    Value left2Right(Value &leftValue);

    void updateMem(Value leftValue, Value rightValue);

    int getPointerLevel (QualType qualType);

    unsigned long getPointeeSize(int pointerLevel);

    void unaryExprOrTypeTraitExpr(UnaryExprOrTypeTraitExpr *expr) {

        if (expr->isArgumentType()) {
            // using sizeof(Type)
            QualType argType = expr->getType();
            int pLevel = getPointerLevel(argType);
            auto pSize = static_cast<int>(getPointeeSize(pLevel));
            mStack.front().bindStmt(expr, Value(pSize));

        } else {
            assert(false);
        }
    }
};


