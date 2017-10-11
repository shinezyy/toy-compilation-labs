//==--- tools/clang-check/ClangInterpreter.cpp - Clang Interpreter tool --------------===//
//===----------------------------------------------------------------------===//
#include <cstdio>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

#include "log.h"

using namespace clang;

enum Typ {
    Int = 0,
    Address,
    Unknown,
};

struct Value {
    const Typ typ;
    union {
        int intValue;
        int *address;
    };

    explicit Value () : typ(Unknown) {
        address = nullptr;
        intValue = 0;
    }

    explicit Value (int intValue_) : typ(Int) {
        address = nullptr;
        intValue = intValue_;
    };

    explicit Value (int *address_) : typ(Address) {
        intValue = 0;
        address = address_;
    };
};

class StackFrame {
    /// StackFrame maps Variable Declaration to Value
    /// Which are either integer or addresses (also represented using an Integer value)
    std::map<Decl *, Value> mVars;
    std::map<Stmt *, Value> mExprs;

    /// The current stmt
    Stmt *mPC;
public:
    StackFrame() : mVars(), mExprs(), mPC() {
    }

    void bindDecl(Decl *decl, Value &value) {
        mVars[decl] = value;
    }

    void bindDecl(Decl *decl, Value &&value) {
        mVars[decl] = value;
    }

    Value getDeclVal(Decl *decl) {
        assert (mVars.find(decl) != mVars.end());
        return mVars.find(decl)->second;
    }

    void bindStmt(Stmt *stmt, Value &value) {
        mExprs[stmt] = value;
    }

    void bindStmt(Stmt *stmt, Value &&value) {
        mExprs[stmt] = value;
    }

    Value getStmtVal(Stmt *stmt) {
        if (IntegerLiteral * IL = dyn_cast<IntegerLiteral>(stmt)) {
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

    void setPC(Stmt *stmt) {
        mPC = stmt;
    }

    Stmt *getPC() {
        return mPC;
    }

    Value returnValue;
};

/// Heap maps address to a value

class Heap {
    std::map<Stmt *, int *> stmtMap;

    std::map<Decl *, int *> declMap;

    std::map<int *, size_t> addressMap;

public:
    int *Malloc(int size) {
        auto *buf = (int *) malloc(static_cast<size_t>(size));
        return buf;
    }

    void Free (int *address) {

    }

    void Update(int *address, int val) {

    }

    int get(int *address) {

    }

    void bindStmt(Stmt *stmt, int *address) {
        stmtMap.insert(std::make_pair(stmt, address));
    }

    void bindDecl(Decl *decl, int *address) {
        declMap.insert(std::make_pair(decl, address));
    }
};


class Environment {
public:
    std::deque<StackFrame> mStack;

private:

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

    template <class T>
    void binResult(BinaryOperator *binaryOperator,
                T &left_value, T &right_value) {
        T result;
        switch (binaryOperator->getOpcode()) {
            case BO_LT: result = left_value < right_value;
                break;
            case BO_GT: result = left_value > right_value;
                break;
            case BO_EQ: result = left_value == right_value;
                break;
            case BO_Add: result = left_value + right_value;
                break;
            case BO_Sub: result = left_value - right_value;
                break;
            default: assert(false);
        }
        mStack.front().bindStmt(binaryOperator, result);

        if (DeclRefExpr *declexpr = dyn_cast<DeclRefExpr>(binaryOperator)) {
            assert(false);
            Decl *decl = declexpr->getFoundDecl();
            mStack.front().bindDecl(decl, result);
        }
    }

    /// !TODO Support comparison operation
    void binop(BinaryOperator *bop) {
        logs(ControlStmt, "Visiting Binary Operator\n");
        Expr *left = bop->getLHS();
        Expr *right = bop->getRHS();

        if (bop->isAssignmentOp()) {
            Value val = mStack.front().getStmtVal(right);
            mStack.front().bindStmt(left, val);
            if (DeclRefExpr *declexpr = dyn_cast<DeclRefExpr>(left)) {
                Decl *decl = declexpr->getFoundDecl();
                mStack.front().bindDecl(decl, val);
            }
            log(ValueBinding, "binding value %d with decl\n", val);

        } else if (bop->isComparisonOp() || bop->isAdditiveOp()) {
            Value left_value = mStack.front().getStmtVal(left);
            Value right_value = mStack.front().getStmtVal(right);
            assert(left_value.typ == right_value.typ);

//            log_var_s(ValueBinding, left_value);
//            log_var_s(ValueBinding, right_value);
            if (left_value.typ == Int) {
                binResult(bop, left_value.intValue, right_value.intValue);
            } else {
                binResult(bop, left_value.address, right_value.address);
            }
        }
    }

    void decl(DeclStmt *declstmt) {
        logs(FunctionCall, "Visiting DeclStmt\n");
        for (DeclStmt::decl_iterator it = declstmt->decl_begin(),
                     ie = declstmt->decl_end();
             it != ie; ++it) {
            Decl *decl = *it;
            if (VarDecl *var_decl = dyn_cast<VarDecl>(decl)) {
                if (Expr *expr = var_decl->getInit()) {
                    if (IntegerLiteral * IL = dyn_cast<IntegerLiteral>(expr)) {
                        // TODO: support negative number
                        int val = static_cast<int>(IL->getValue().getLimitedValue());
                        mStack.front().bindDecl(var_decl, Value(val));
                    }
                } else {
                    // TODO: Let consumer know "This is default value, not initialized"
                    mStack.front().bindDecl(var_decl, Value(0));
                }
            }
        }
    }

    void declref(DeclRefExpr *declref) {
        mStack.front().setPC(declref);
        if (declref->getType()->isIntegerType()) {
            if (Decl *decl = declref->getFoundDecl()) {
                Value val = mStack.front().getDeclVal(decl);
                mStack.front().bindStmt(declref, val);
            }
        }
    }

    void cast(CastExpr *castexpr) {
        mStack.front().setPC(castexpr);
        if (castexpr->getType()->isIntegerType()) {
            Expr *expr = castexpr->getSubExpr();
            Value val = mStack.front().getStmtVal(expr);
            mStack.front().bindStmt(castexpr, val);
        }
    }

    /// !TODO Support Function Call
    FunctionDecl * call(CallExpr *callexpr) {
        mStack.front().setPC(callexpr);
        Value val;
        FunctionDecl *callee = callexpr->getDirectCallee();
        logs(FunctionCall, "Visiting function call\n");
        if (callee == mInput) {
            llvm::errs() << "Please Input an Integer Value : ";
            scanf("%d", &val);

            mStack.front().bindStmt(callexpr, Value(val));
            return nullptr;

        } else if (callee == mOutput) {
            Expr *arg = callexpr->getArg(0);
            val = mStack.front().getStmtVal(arg);
            assert(val.typ == Int);
            llvm::errs() << val.intValue;
            return nullptr;

        } else {
            /// You could add your code here for Function call Return
            StackFrame &curStackFrame = mStack.front();

            // DONE: Need a flag to indicate whether it has been visited

            // sub-procedure's stack frame
            mStack.push_front(StackFrame());
            for (unsigned i = 0, n = callexpr->getNumArgs(); i < n; ++i) {
                log(FunctionCall, "Preparing Parameter[%i]\n", i);

                Expr *arg = callexpr->getArg(i);
                val = curStackFrame.getStmtVal(arg);

                Decl *decl = dyn_cast<Decl>(callee->getParamDecl(i));
                assert(decl);

                mStack.front().bindDecl(decl, val);
            }
            return callee;
        }
    }

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
};


