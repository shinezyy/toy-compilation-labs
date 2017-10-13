//
// Created by diamo on 2017/10/13.
//

#include "Environment.h"


void Environment::unaryOp(UnaryOperator *unaryOperator) {
    logs(PointerVisit, "Visiting Unary Operator\n");
    Expr *subExpr = unaryOperator->getSubExpr();

    if (unaryOperator->getOpcode() == UO_Deref) {
        Value value = mStack.front().getStmtVal(subExpr);
        logp(PointerVisit, value.address);
        mStack.front().bindStmt(unaryOperator, value);

    } else if (unaryOperator->getOpcode() == UO_Minus) {
        Value value = mStack.front().getStmtVal(subExpr);
        value.intValue = -value.intValue;
        mStack.front().bindStmt(unaryOperator, value);
    }
    logs(PointerVisit, "Visited Unary Operator\n");
}
