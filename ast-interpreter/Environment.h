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

class StackFrame {
    /// StackFrame maps Variable Declaration to Value
    /// Which are either integer or addresses (also represented using an Integer value)
    std::map<Decl *, int> mVars;
    std::map<Stmt *, int> mExprs;

    /// The current stmt
    Stmt *mPC;
public:
    StackFrame() : mVars(), mExprs(), mPC() {
    }

    void bindDecl(Decl *decl, int val) {
        mVars[decl] = val;
    }

    int getDeclVal(Decl *decl) {
        assert (mVars.find(decl) != mVars.end());
        return mVars.find(decl)->second;
    }

    void bindStmt(Stmt *stmt, int val) {
        mExprs[stmt] = val;
    }

    int getStmtVal(Stmt *stmt) {
        if (IntegerLiteral * IL = dyn_cast<IntegerLiteral>(stmt)) {
            int val = static_cast<int>(IL->getValue().getLimitedValue());
            return val;
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
};

/// Heap maps address to a value
/*
class Heap {
   /// The allocated bufs, key is the address, val is its size
   std::map<int, int> mBufs;
   /// The map that maps address to value
   std::map<int, int> mContents;
public:
   int Malloc(int size) {
      assert (mBufs.find(addr) == mHeap.end());
      /// Allocate the buf
      int * buf = malloc(size * sizeof(int) );
      mBufs.insert(std::make_pair(addr, size));

      /// Initialize the Content
      for (int i=0; i<size; i ++) {
         mContents.insert(std::make_pair(buf+i, 0));
      }
      return buf;
   }
   void Free (int addr) {
      /// Clear the contents;
      assert (mHeap.find(addr) != mHeap.end());
      int * buf = addr;
      int size = mHeap.find(addr)->second;
      for (int i = 0; i < size; i++) {
          assert (mContents.find(buf+i) != mContents.end());
          mContents.erase(buf+i);
      }
        /// Free the allocated buf
      free(mHeap.find(addr)->second);
   }
   void Update(int addr, int val) {
      assert (mContents.find(addr) != mContents.end());
      mContents[addr] = val;
   }
   int get(int addr) {
      assert (mContents.find(addr) != mContents.end());
      return mContents.find(addr)->second;
    }
};
*/

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

    /// !TODO Support comparison operation
    void binop(BinaryOperator *bop) {
        logs(ControlStmt, "Visiting Binary Operator\n");
        Expr *left = bop->getLHS();
        Expr *right = bop->getRHS();

        if (bop->isAssignmentOp()) {
            int val = mStack.front().getStmtVal(right);
            mStack.front().bindStmt(left, val);
            if (DeclRefExpr *declexpr = dyn_cast<DeclRefExpr>(left)) {
                Decl *decl = declexpr->getFoundDecl();
                mStack.front().bindDecl(decl, val);
            }
            log(ValueBinding, "binding value %d with decl\n", val);

        } else if (bop->isComparisonOp() || bop->isAdditiveOp()) {
            int left_value = mStack.front().getStmtVal(left);
            int right_value = mStack.front().getStmtVal(right);
            int result;
            log_var_s(ValueBinding, left_value);
            log_var_s(ValueBinding, right_value);
            switch (bop->getOpcode()) {
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
            mStack.front().bindStmt(bop, result);
            if (DeclRefExpr *declexpr = dyn_cast<DeclRefExpr>(bop)) {
                assert(false);
                Decl *decl = declexpr->getFoundDecl();
                mStack.front().bindDecl(decl, result);
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
                        mStack.front().bindDecl(var_decl, val);
                    }
                } else {
                    // TODO: Let consumer know "This is default value, not initialized"
                    mStack.front().bindDecl(var_decl, 0);
                }
            }
        }
    }

    void declref(DeclRefExpr *declref) {
        mStack.front().setPC(declref);
        if (declref->getType()->isIntegerType()) {
            if (Decl *decl = declref->getFoundDecl()) {
                int val = mStack.front().getDeclVal(decl);
                mStack.front().bindStmt(declref, val);
            }
        }
    }

    void cast(CastExpr *castexpr) {
        mStack.front().setPC(castexpr);
        if (castexpr->getType()->isIntegerType()) {
            Expr *expr = castexpr->getSubExpr();
            int val = mStack.front().getStmtVal(expr);
            mStack.front().bindStmt(castexpr, val);
        }
    }

    /// !TODO Support Function Call
    FunctionDecl * call(CallExpr *callexpr) {
        mStack.front().setPC(callexpr);
        int val = 0;
        FunctionDecl *callee = callexpr->getDirectCallee();
        logs(FunctionCall, "Visiting function call\n");
        if (callee == mInput) {
            llvm::errs() << "Please Input an Integer Value : ";
            scanf("%d", &val);

            mStack.front().bindStmt(callexpr, val);
            return nullptr;

        } else if (callee == mOutput) {
            Expr *arg = callexpr->getArg(0);
            val = mStack.front().getStmtVal(arg);
            llvm::errs() << val;
            return nullptr;

        } else {
            /// You could add your code here for Function call Return
            StackFrame &curStackFrame = mStack.front();

            // TODO: Need a flag to indicate whether it has been visited

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
            // TODO: bind return value with stmt
            return callee;
        }
    }

    void parmDecl(ParmVarDecl *parmVarDecl) {
        logs(FunctionCall, "Visting Param Decl Stmt\n");
        // TODO: iterate through the param list, and set decl values
        assert(false);
    }
};


