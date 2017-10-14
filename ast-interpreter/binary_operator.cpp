//
// Created by diamo on 2017/10/12.
//

#include "Environment.h"

Value compute(Value &l, Value &r, std::function<int(int, int)> bop_int,
              std::function<unsigned long(unsigned long, unsigned long)> bop_long) {

//    assert((l.typ != LeftValue) && (r.typ != LeftValue));
//    assert(!(l.typ == Address && r.typ == Address));

    if (l.typ == Int && r.typ == Int) {
        Value val = r;
        val.intValue = bop_int(l.intValue, r.intValue);
        return val;
    } else {
        Value *lp = &l, *rp = &r;
        makeLeftAddress(lp, rp);
        assert(lp->pointeeSize);
        Value val = *lp;
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


void Environment::binOp(BinaryOperator *bop) {
    logs(PointerVisit, "Visiting Binary Operator\n");
    Expr *left = bop->getLHS();
    Expr *right = bop->getRHS();

    if (bop->isAssignmentOp()) {

        // TODO: consider lvalue on the right
        Value rightValue = mStack.front().getStmtVal(right);

        if (DeclRefExpr *declExpr = dyn_cast<DeclRefExpr>(left)) {
            Decl *decl = declExpr->getFoundDecl();
            if (!getDeclType(decl)->isPointerType()) {
                mStack.front().bindDecl(decl, rightValue);
                log(ValueBinding, "Binding %s with int: %d\n",
                    getDeclstr(decl), rightValue.intValue);

            } else {
                mStack.front().bindDecl(decl, Value(rightValue.address));
                log(ValueBinding, "Binding %s with address: %p\n",
                    getDeclstr(decl), rightValue.address);
            }

        } else if (auto unaryOperator = dyn_cast<UnaryOperator>(left)){
            if (unaryOperator->getOpcode() == UO_Deref) {
                size_t pointee_size = getDeRefPointeeSize(unaryOperator);
                log(PointerVisit, "Updating size: %lu\n", pointee_size);
                updateMem(mStack.front().getStmtVal(left), rightValue, pointee_size);
            }

        } else if (auto array = dyn_cast<ArraySubscriptExpr>(left)){
            size_t member_size = getArrayMemberSize(array);
            log(ArrayVisit, "Updating size: %lu\n", member_size);
            updateMem(mStack.front().getStmtVal(left), rightValue, member_size);
        }

        mStack.front().bindStmt(bop, rightValue);

    } else if (bop->isComparisonOp() || bop->isAdditiveOp()
               || bop->isMultiplicativeOp()) {
        Value left_value = mStack.front().getStmtVal(left);
        Value right_value = mStack.front().getStmtVal(right);
//        assert(left_value.typ == right_value.typ);
//        assert(getExprType(left) == getExprType(right));

        // Make sure pointee size is only used here and arrayDeRef
        int p_level = getPointerLevel(getExprType(left));
        if (p_level > 1) {
            left_value.pointeeSize = 8;
            right_value.pointeeSize = 8;
        } else if (p_level == 1) {
            left_value.pointeeSize = 4;
            right_value.pointeeSize = 4;
        } else {
            assert(left_value.typ == Int);
            assert(right_value.typ == Int);
        }
        DispatchComputation(bop, left_value, right_value);
    }
    logs(PointerVisit, "Visited Binary Operator\n");
}

void Environment::arrayDeRef(ArraySubscriptExpr *arraySubscriptExpr) {
    Expr *left = arraySubscriptExpr->getBase();
    Expr *right = arraySubscriptExpr->getIdx();

    Value left_value = mStack.front().getStmtVal(left);
    Value right_value = mStack.front().getStmtVal(right);

    int p_level = getPointerLevel(getExprType(left));
    if (p_level > 1) {
        left_value.pointeeSize = 8;
    } else if (p_level == 1) {
        left_value.pointeeSize = 4;
    } else {
        assert(false);
    }
    Value val = compute(left_value, right_value, lambda_helper(+));
    mStack.front().bindStmt(arraySubscriptExpr, val);
}


#undef lambda_helper
