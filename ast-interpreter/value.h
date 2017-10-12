//
// Created by diamo on 2017/10/12.
//

#ifndef CLANG_DEVEL_CLION_VALUE_H
#define CLANG_DEVEL_CLION_VALUE_H


enum Typ {
    Int = 0,
    Address,
    LeftValue,
    Unknown,
};

struct Value {
    Typ typ;
    int pointerLevel;
    unsigned long pointeeSize;

    union {
        int intValue;
        void *address;
    };

    explicit Value () : typ(Unknown) {
        pointerLevel = -1;
    }

    explicit Value (int intValue_) : typ(Int) {
        pointerLevel = -1;
        intValue = intValue_;
    };

    explicit Value (void *address_) : typ(Address) {
        pointerLevel = -1;
        address = address_;
    };

//    explicit Value (Value& val) : typ(val.typ) {
//        pointerLevel = val.pointerLevel;
//        pointeeSize = val.pointeeSize;
//    };

    Value operator=(Value val);
};

void makeLeftAddress(Value* &l, Value* &r);


#endif //CLANG_DEVEL_CLION_VALUE_H
