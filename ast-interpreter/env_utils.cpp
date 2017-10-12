//
// Created by diamo on 2017/10/12.
//

#include "Environment.h"

Value Environment::deRef(Value &value) {
    assert(value.pointerLevel > 0);
    Value ret = value;
    ret.typ = LeftValue;
    return ret;
}

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

Value Environment::left2Right(Value &leftValue) {
    assert(leftValue.typ == LeftValue);
    assert(leftValue.pointerLevel >= 1);

    Value ret;
    if (leftValue.pointerLevel == 1) {
        ret.typ = Int;
        ret.pointerLevel = 0;
        ret.intValue = heap.get((int *) leftValue.address);
    } else {
        ret.typ = Address;
        ret.pointerLevel = leftValue.pointerLevel - 1;
        ret.address = heap.get((void **)leftValue.address);
    }
    return ret;
}


void Environment::updateMem(Value leftValue, Value rightValue) {
    assert(leftValue.typ == LeftValue);
    assert(rightValue.typ != LeftValue);
    assert(leftValue.pointerLevel >= 1);

    if (leftValue.pointerLevel == 1) {
        logp(PointerVisit, leftValue.address);
        log_var(PointerVisit, leftValue.pointerLevel);
        heap.Update((int *) leftValue.address, rightValue.intValue);
    } else {
        heap.Update((void **) leftValue.address, rightValue.address);
    }
}

