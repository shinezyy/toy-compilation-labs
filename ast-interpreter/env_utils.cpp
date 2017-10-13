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
    if (size == 32) {
        heap.Update((int *) leftValue.address, rightValue.intValue);
    } else {
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
