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
    LeftValue,
    Unknown,
};

struct Value {
    Typ typ;
    int pointerLevel;
    unsigned long pointeeSize;

    union {
        int intValue;
        void *address;
    };

    explicit Value () : typ(Unknown) {
        pointerLevel = -1;
    }

    explicit Value (int intValue_) : typ(Int) {
        pointerLevel = -1;
        intValue = intValue_;
    };

    explicit Value (void *address_) : typ(Address) {
        pointerLevel = -1;
        address = address_;
    };

    explicit Value (Value val) : typ(val.typ) {
        pointerLevel = val.pointerLevel;
        pointeeSize = val.pointeeSize;
    };

    Value &operator=(Value val) {
        typ = val.typ;
        pointerLevel = val.pointerLevel;
        pointeeSize = val.pointeeSize;
        address = val.address;
    }
};

void makeLeftAddress(Value* &l, Value* &r) {
    if (l->typ == Address) {
        return;
    } else {
        Value *tmp;
        tmp = l;
        l = r;
        r = tmp;
    }
}

template<class T>
Value compute(Value &l, Value &r, std::function< T&(T&, T&) > lambda) {
    assert((l.typ != LeftValue) && (r.typ != LeftValue));
    assert(!(l.typ == Address && r.typ == Address));

    if (l.typ == Int && r.typ == Int) {
        Value val(r);
        val.intValue = lambda(l.intValue, r.intValue);
        return val;
    } else {
        Value *lp = &l, *rp = &r;
        makeLeftAddress(lp, rp);
        Value val(*lp);
        unsigned long result =
                lambda((unsigned long) l.address,
                       ((unsigned long) r.intValue) * l.pointeeSize);
        val.address = (void *) result;
        return val;
    }
}


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
    std::map<void *, size_t> addressMap;

public:
    void *Malloc(int size) {
        auto *buf = (void *) malloc(static_cast<size_t>(size));
        return buf;
    }

    void Free (void *address) {
        assert(noWild(address));
        free((void *) address);
    }

    void Update(int *address, int val) {
        assert(noWild(address));
        *address = val;
    }

    void Update(void **address, void *val) {
        assert(noWild(address));
        *address = val;
    }

    bool noWild(void *address) {
        auto lower = addressMap.lower_bound(address);
        assert(lower != addressMap.end());
        assert(lower->first + lower->second > address);
        return true;
    }

    int get(int *address) {
        assert(noWild(address));
        return *address;
    }

    void *get(void **address) {
        assert(noWild(address));
        return *address;
    }
};


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

    template <class T>
    void DispatchComputation(BinaryOperator *binaryOperator, Value &x, Value &y) {
        Value result;
        switch (binaryOperator->getOpcode()) {
            case BO_Add: result = compute(x, y, [](T& a, T& b) {return a+b;});
                break;
            case BO_Sub: result = compute(x, y, [](T& a, T& b) {return a-b;});
                break;
            case BO_Mul: result = compute(x, y, [](T& a, T& b) {return a*b;});
                break;
            case BO_Div: result = compute(x, y, [](T& a, T& b) {return a/b;});
                break;
            case BO_EQ: result = compute(x, y, [](T& a, T& b) {return a==b;});
                break;
            case BO_GT: result = compute(x, y, [](T& a, T& b) {return a>b;});
                break;
            case BO_LT: result = compute(x, y, [](T& a, T& b) {return a<b;});
                break;
            default: assert(false);
        }
        mStack.front().bindStmt(binaryOperator, Value(result));
    }

    /// !TODO Support comparison operation
    void binOp(BinaryOperator *bop) {
        logs(PointerVisit, "Visiting Binary Operator\n");
        Expr *left = bop->getLHS();
        Expr *right = bop->getRHS();

        if (bop->isAssignmentOp()) {

            // TODO: consider lvalue on the right
            Value rightValue = mStack.front().getStmtVal(right);
            assert(rightValue.typ != LeftValue);

            if (DeclRefExpr *declExpr = dyn_cast<DeclRefExpr>(left)) {
                Decl *decl = declExpr->getFoundDecl();
                mStack.front().bindDecl(decl, rightValue);

            } else if (UnaryOperator *unaryOperator = dyn_cast<UnaryOperator>(left)){
                updateMem(mStack.front().getStmtVal(left), rightValue);
            }

            mStack.front().bindStmt(bop, rightValue);

        } else if (bop->isComparisonOp() || bop->isAdditiveOp()
                || bop->isMultiplicativeOp()) {
            Value left_value = mStack.front().getStmtVal(left);
            Value right_value = mStack.front().getStmtVal(right);
            assert(left_value.typ == right_value.typ);

//            log_var_s(ValueBinding, left_value);
//            log_var_s(ValueBinding, right_value);
            DispatchComputation(bop, left_value, right_value);
        }
    }

    Value deRef(Value &value) {
        assert(value.pointerLevel > 0);
        Value ret = value;
        ret.typ = LeftValue;
        return ret;
    }

    void unaryOp(UnaryOperator *unaryOperator) {
        logs(PointerVisit, "Visiting Unary Operator\n");
        Expr *subExpr = unaryOperator->getSubExpr();

        if (unaryOperator->getOpcode() == UO_Deref) {
            Value value = mStack.front().getStmtVal(subExpr);
            mStack.front().bindStmt(unaryOperator, deRef(value));
        }
        logs(PointerVisit, "Visited Unary Operator\n");
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
                        break;
                    }
                }
                // TODO: Let consumer know "This is default value, not initialized"
                // getInit is null
                int pLevel = getPointerLevel(var_decl->getType());
                unsigned long pSize = getPointeeSize(pLevel);

                Value value;
                if (pLevel == 0) {
                    logs(PointerVisit, "Not pointer");
                    value.typ = Int;
                    value.pointerLevel = 0;
                } else {
                    log(PointerVisit, "Lv %i pointer\n", pLevel);
                    value.typ = Address;
                    value.pointerLevel = pLevel;
                    value.pointeeSize = pSize;
                }
                mStack.front().bindDecl(var_decl, value);
            }
        }
    }

    void declRef(DeclRefExpr *declRefexpr) {
        mStack.front().setPC(declRefexpr);
        QualType qualType = declRefexpr->getType();
        Decl *decl = declRefexpr->getFoundDecl();

        if (qualType->isPointerType()) {
            assert(decl);
            Value val = mStack.front().getDeclVal(decl);
            mStack.front().bindStmt(declRefexpr, val);

        } else if (declRefexpr->getType()->isIntegerType()) {
            if (decl) {
                Value val = mStack.front().getDeclVal(decl);
                mStack.front().bindStmt(declRefexpr, val);
            }
        }
    }

    void cast(CastExpr *castExpr) {
        logs(PointerVisit, "Visiting cast expr\n");
        mStack.front().setPC(castExpr);

        if (castExpr->getType()->isIntegerType()) {
            Expr *expr = castExpr->getSubExpr();
            Value val = mStack.front().getStmtVal(expr);
            mStack.front().bindStmt(castExpr, val);
        }
    }

    /// !TODO Support Function Call
    FunctionDecl * call(CallExpr *callExpr) {
        mStack.front().setPC(callExpr);
        Value val;
        FunctionDecl *callee = callExpr->getDirectCallee();
        logs(FunctionCall, "Visiting function call\n");
        if (callee == mInput) {
            val.typ = Int;
            llvm::errs() << "Please Input an Integer Value : ";
            scanf("%d", &val.intValue);

            mStack.front().bindStmt(callExpr, Value(val));
            return nullptr;

        } else if (callee == mOutput) {
            Expr *arg = callExpr->getArg(0);
            val = mStack.front().getStmtVal(arg);

            assert(val.typ == Int);
            llvm::errs() << val.intValue;
            return nullptr;

        } else if (callee == mMalloc){
            Expr *arg= callExpr->getArg(0);
            val = mStack.front().getStmtVal(arg);

            assert(val.typ == Int);
            // val.intValue is allocation size
            void *allocatedAddress = heap.Malloc(val.intValue);
            mStack.front().bindStmt(callExpr, Value(allocatedAddress));

            return nullptr;

        } else if (callee == mFree){
            Expr *arg= callExpr->getArg(0);
            val = mStack.front().getStmtVal(arg);

            assert(val.typ == Address);
            heap.Free((int *)val.address);

            return nullptr;

        } else {
            /// You could add your code here for Function call Return
            StackFrame &curStackFrame = mStack.front();

            // DONE: Need a flag to indicate whether it has been visited

            // sub-procedure's stack frame
            mStack.push_front(StackFrame());
            for (unsigned i = 0, n = callExpr->getNumArgs(); i < n; ++i) {
                log(FunctionCall, "Preparing Parameter[%i]\n", i);

                Expr *arg = callExpr->getArg(i);
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

    void implicitCast(ImplicitCastExpr *implicitCastExpr) {
        Value value = mStack.front().getStmtVal(implicitCastExpr->getSubExpr());
        if (implicitCastExpr->isRValue() && value.typ == LeftValue) {
            mStack.front().bindStmt(implicitCastExpr, left2Right(value));
        } else {
            mStack.front().bindStmt(implicitCastExpr, value);
        }
    }

    Value left2Right(Value &leftValue) {
        assert(leftValue.typ == LeftValue);
        assert(leftValue.pointerLevel >= 1);

        Value ret;
        if (leftValue.pointerLevel == 1) {
            ret.typ = Int;
            ret.pointerLevel = 0;
            ret.intValue = heap.get((int *) leftValue.address);
        } else {
            ret.typ = Address;
            ret.pointerLevel = leftValue.pointerLevel - 1;
            ret.address = heap.get((void **)leftValue.address);
        }
        return ret;
    }

    void updateMem(Value leftValue, Value rightValue) {
        assert(leftValue.typ == LeftValue);
        assert(rightValue.typ != LeftValue);
        assert(leftValue.pointerLevel >= 1);

        if (leftValue.pointerLevel == 1) {
            heap.Update((int *) leftValue.address, rightValue.intValue);
        } else {
            heap.Update((void **) leftValue.address, rightValue.address);
        }
    }

    int getPointerLevel (QualType qualType) {
        if (!qualType->isPointerType()) {
            return 0;
        } else {
            return 1 + getPointerLevel(qualType->getPointeeType());
        }
    }

    unsigned long getPointeeSize(int pointerLevel) {
        if (pointerLevel == 0) {
            return 0;
        } else if (pointerLevel == 1) {
            return sizeof(int);
        } else {
            return sizeof(int *);
        }
    }

};


