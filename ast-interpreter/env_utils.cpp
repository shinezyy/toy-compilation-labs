//
// Created by diamo on 2017/10/12.
//

#include "Environment.h"

int Environment::getPointerLevel(clang::QualType qualType) {
    if (!qualType->isPointerType()) {
        return 0;
    } else {
        return 1 + getPointerLevel(qualType->getPointeeType());
    }
}

unsigned long Environment::getPointeeSize(int pointerLevel) {
    if (pointerLevel == 0) {
        return 0;
    } else if (pointerLevel == 1) {
        return sizeof(int);
    } else {
        return sizeof(int *);
    }
}

void Environment::updateMem(Value leftValue, Value rightValue, size_t size) {
    if (size == 4) {
        heap.Update((int *) leftValue.address, rightValue.intValue);
    } else {
        assert(size == 8);
        heap.Update((void **) leftValue.address, rightValue.address);
    }
}

QualType Environment::getDeclType(Decl *decl) {
    if (VarDecl *var_decl = dyn_cast<VarDecl>(decl)) {
        return var_decl->getType();
    }
    assert(false);
}

QualType Environment::getExprType(Expr *expr) {
    return expr->getType();
}

TypeInfo Environment::getTypeInfo(Expr *expr) {
    QualType qualType = expr->getType();
    TypeInfo typeInfo = context.getTypeInfo(qualType);
    return typeInfo;
}

const char *Environment::getDeclstr(Decl *decl) {
    if (VarDecl *var_decl = dyn_cast<VarDecl>(decl)) {
        return var_decl->getIdentifier()->getName().str().c_str();
    }
    return nullptr;
}

bool Environment::isDerefExpr(Expr *expr) {
    auto uop = dyn_cast<UnaryOperator>(expr);
    if (!uop) {
        return false;
    }
    return uop->getOpcode() == UO_Deref;
}

size_t Environment::getDeRefPointeeSize(UnaryOperator *unaryOperator) {
    assert(isDerefExpr(unaryOperator));

    Expr *sub_expr = unaryOperator->getSubExpr();
    QualType sub_expr_type = getExprType(sub_expr);

    size_t size = getPointeeSize(getPointerLevel(sub_expr_type));
    return size;

}

size_t Environment::getArrayMemberSize(ArraySubscriptExpr *array) {
    Expr *left = array->getBase();
    auto array_type = left->getType();
    auto member_type = context.getTypeInfo(array_type);
    log_var(ArrayVisit, (int) member_type.Width);
    return member_type.Width / 8;
}


