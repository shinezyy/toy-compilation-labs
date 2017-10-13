//
// Created by diamo on 2017/10/12.
//

#include "Environment.h"

void Environment::implicitCast(ImplicitCastExpr *implicitCastExpr) {

    CastKind castKind = implicitCastExpr->getCastKind();
    Expr *castedExpr = implicitCastExpr->getSubExpr();

    const char *castKindStr = implicitCastExpr->getCastKindName();
    log(CastVisit, "Implicit kind: %s\n", castKindStr);

    if (castKind == CK_LValueToRValue) {
        Value value = mStack.front().getStmtVal(castedExpr);

        auto unary_operator = dyn_cast<UnaryOperator>(castedExpr);
        auto decl_ref = dyn_cast<DeclRefExpr>(castedExpr);

        if (decl_ref) {
            log(CastVisit, "cast var %s into RValue\n",
                getDeclstr(decl_ref->getFoundDecl()));
            mStack.front().bindStmt(implicitCastExpr, value);

        } else if (unary_operator) {
            logp(PointerVisit || CastVisit, value.address);
            if (isDerefExpr(unary_operator)) {
                auto pointee_size = getDeRefPointeeSize(unary_operator);
                log_var(PointerVisit || CastVisit, (int) pointee_size);
                if (pointee_size == 4) {
                    auto x = heap.get((int *)value.address);
                    mStack.front().bindStmt(implicitCastExpr, Value(x));
                } else {
                    assert(pointee_size == 8);
                    auto x = heap.get((void **)value.address);
                    mStack.front().bindStmt(implicitCastExpr, Value(x));
                }
            }

        } else {
            logs(CastVisit, "Unknown implicit cast\n");
        }

    } else if (castKind == CK_FunctionToPointerDecay){
        return;

    } else if (castKind == CK_IntegralCast){
        Value value = mStack.front().getStmtVal(castedExpr);
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

void Environment::CStyleCast(CStyleCastExpr *castExpr) {
    logs(PointerVisit, "Visiting C style cast expr\n");

    Expr *expr = castExpr->getSubExpr();
    Value val = mStack.front().getStmtVal(expr);
    mStack.front().bindStmt(castExpr, val);
}

