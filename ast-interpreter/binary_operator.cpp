//
// Created by diamo on 2017/10/12.
//

#include "Environment.h"

Value compute(Value &l, Value &r, std::function<int(int, int)> bop_int,
              std::function<unsigned long(unsigned long, unsigned long)> bop_long) {

    assert((l.typ != LeftValue) && (r.typ != LeftValue));
    assert(!(l.typ == Address && r.typ == Address));

    if (l.typ == Int && r.typ == Int) {
        Value val = r;
        val.intValue = bop_int(l.intValue, r.intValue);
        return val;
    } else {
        Value *lp = &l, *rp = &r;
        makeLeftAddress(lp, rp);
        Value val(*lp);
        unsigned long result =
                bop_long((unsigned long) lp->address,
                         ((unsigned long) rp->intValue) * lp->pointeeSize);
        val.address = (void *) result;
        return val;
    }
}

#define lambda_helper(op) \
        [](int a, int b) {return a op b;}, \
        [](unsigned long a, unsigned long b) {return a op b;}

void Environment::DispatchComputation(BinaryOperator *binaryOperator, Value &x, Value &y) {
    Value result;

    switch (binaryOperator->getOpcode()) {
        case BO_Add: result = compute(x, y, lambda_helper(+));
            break;
        case BO_Sub: result = compute(x, y, lambda_helper(-));
            break;
        case BO_Mul: result = compute(x, y, lambda_helper(*));
            break;
        case BO_Div: result = compute(x, y, lambda_helper(/));
            break;
        case BO_EQ: result = compute(x, y, lambda_helper(==));
            break;
        case BO_GT: result = compute(x, y, lambda_helper(>));
            break;
        case BO_LT: result = compute(x, y, lambda_helper(<));
            break;
        default: assert(false);
    }
    mStack.front().bindStmt(binaryOperator, Value(result));
}

#undef lambda_helper

void Environment::binOp(BinaryOperator *bop) {
    logs(PointerVisit, "Visiting Binary Operator\n");
    Expr *left = bop->getLHS();
    Expr *right = bop->getRHS();

    if (bop->isAssignmentOp()) {

        // TODO: consider lvalue on the right
        Value rightValue = mStack.front().getStmtVal(right);
        assert(rightValue.typ != LeftValue);

        if (DeclRefExpr *declExpr = dyn_cast<DeclRefExpr>(left)) {
            Decl *decl = declExpr->getFoundDecl();
            if (!left->getType()->isPointerType()) {
                mStack.front().bindDecl(decl, rightValue);
            } else {
                Value leftValue = mStack.front().getStmtVal(left);
                Value newValue(leftValue);
                newValue.address = rightValue.address;
                mStack.front().bindDecl(decl, newValue);
            }

        } else if (UnaryOperator *unaryOperator = dyn_cast<UnaryOperator>(left)){
            if (unaryOperator->getOpcode() == UO_Deref) {
                updateMem(mStack.front().getStmtVal(left), rightValue);
            }
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
    logs(PointerVisit, "Visited Binary Operator\n");
}


