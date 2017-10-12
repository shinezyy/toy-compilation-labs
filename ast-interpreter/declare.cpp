//
// Created by diamo on 2017/10/12.
//

#include "Environment.h"

void Environment::declRef(DeclRefExpr *declRefexpr) {
    mStack.front().setPC(declRefexpr);
    QualType qualType = declRefexpr->getType();
    Decl *decl = declRefexpr->getFoundDecl();

    if (qualType->isPointerType()) {
        assert(decl);
        Value val = mStack.front().getDeclVal(decl);
        logp(PointerVisit, val.address);
        log_var(PointerVisit, val.pointerLevel);
        mStack.front().bindStmt(declRefexpr, val);

    } else if (declRefexpr->getType()->isIntegerType()) {
        if (decl) {
            Value val = mStack.front().getDeclVal(decl);
            log_var(DeclVisit, val.intValue);
            mStack.front().bindStmt(declRefexpr, val);
        }
    }
}

void Environment::decl(DeclStmt *declstmt) {
    logs(FunctionCall, "Visiting DeclStmt\n");
    for (DeclGroupRef::iterator it = declstmt->decl_begin(),
                 ie = declstmt->decl_end();
         it != ie; ++it) {
        Decl *decl = *it;
        if (VarDecl *var_decl = dyn_cast<VarDecl>(decl)) {
            Expr *initExpr = var_decl->getInit();

            //<editor-fold desc="Integer init">
            if (initExpr) {
                if (IntegerLiteral * IL = dyn_cast<IntegerLiteral>(initExpr)) {
                    // TODO: support negative number
                    int val = static_cast<int>(IL->getValue().getLimitedValue());
                    log_var(DeclVisit, val);
                    mStack.front().bindDecl(var_decl, Value(val));
                    continue;
                }
            }
            //</editor-fold>

            // TODO: Let consumer know "This is default value, not initialized"

            int pLevel = getPointerLevel(var_decl->getType());
            unsigned long pSize = getPointeeSize(pLevel);

            Value default_value;
            if (pLevel == 0) {
                logs(PointerVisit, "Not pointer\n");
                default_value.typ = Int;
                default_value.pointerLevel = 0;
                mStack.front().bindDecl(var_decl, default_value);
            } else {
                log(PointerVisit, "Lv %i pointer\n", pLevel);

                if (initExpr) {
                    Value initVal = mStack.front().getStmtVal(initExpr);

                    // pointer info can only be obtained from here
                    initVal.pointerLevel = pLevel;
                    initVal.pointeeSize = pSize;

                    logp(PointerVisit, initVal.address);
                    mStack.front().bindDecl(var_decl, initVal);

                } else {
                    default_value.typ = Address;
                    default_value.pointerLevel = pLevel;
                    default_value.pointeeSize = pSize;
                    mStack.front().bindDecl(var_decl, default_value);
                }
            }
        }
    }
}