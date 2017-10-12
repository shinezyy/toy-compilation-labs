//
// Created by diamo on 2017/10/12.
//

#include "value.h"

void makeLeftAddress(Value *&l, Value *&r) {
    if (l->typ == Address) {
        return;
    } else {
        Value *tmp;
        tmp = l;
        l = r;
        r = tmp;
    }
}

Value Value::operator=(Value val) {
    typ = val.typ;
    pointerLevel = val.pointerLevel;
    pointeeSize = val.pointeeSize;
    address = val.address;
    return *this;
}
