//
// Created by diamo on 2017/10/12.
//

#include "Environment.h"

void Environment::implicitCast(ImplicitCastExpr *implicitCastExpr) {

    CastKind castKind = implicitCastExpr->getCastKind();
    Expr *subExpr = implicitCastExpr->getSubExpr();

    const char *castKindStr = implicitCastExpr->getCastKindName();
    log(CastVisit, "Implicit kind: %s\n", castKindStr);

    if (castKind == CK_LValueToRValue) {
        Value value = mStack.front().getStmtVal(subExpr);

        logp(PointerVisit, value.address);
        log_var(PointerVisit, value.pointerLevel);

        if (implicitCastExpr->isRValue() && value.typ == LeftValue) {
            mStack.front().bindStmt(implicitCastExpr, left2Right(value));
        } else {
            mStack.front().bindStmt(implicitCastExpr, value);
        }

    } else if (castKind == CK_FunctionToPointerDecay){
        return;

    } else if (castKind == CK_IntegralCast){
        Value value = mStack.front().getStmtVal(subExpr);
        mStack.front().bindStmt(implicitCastExpr, value);

    } else {
        assert(false);
    }
}

void Environment::cast(CastExpr *castExpr) {
    logs(PointerVisit, "Visiting cast expr\n");
    mStack.front().setPC(castExpr);

    if (castExpr->getType()->isIntegerType()) {
        Expr *expr = castExpr->getSubExpr();
        Value val = mStack.front().getStmtVal(expr);
        mStack.front().bindStmt(castExpr, val);
    }
}

