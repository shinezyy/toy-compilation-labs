//
// Created by diamo on 2017/10/12.
//

#include "Environment.h"

FunctionDecl *Environment::call(CallExpr *callExpr) {
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
        logp(PointerVisit, allocatedAddress);

        mStack.front().bindStmt(callExpr, Value(allocatedAddress));
        logp(PointerVisit, (void *) callExpr);
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

