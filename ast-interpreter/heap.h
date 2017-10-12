//
// Created by diamo on 2017/10/12.
//

#ifndef CLANG_DEVEL_CLION_HEAP_H
#define CLANG_DEVEL_CLION_HEAP_H

class Heap {
    std::map<void *, size_t> addressMap;

public:
    void *Malloc(int size) {
        auto *buf = (void *) malloc(static_cast<size_t>(size));
        addressMap.insert(std::make_pair(buf, size));
        return buf;
    }

    void Free (void *address) {
        assert(noWild(address));
        free((void *) address);
    }

    void Update(int *address, int val) {
        assert(noWild(address));
        *address = val;
    }

    void Update(void **address, void *val) {
        assert(noWild(address));
        *address = val;
    }

    bool noWild(void *address) {
        auto lower = addressMap.lower_bound(address);
        log(PointerVisit, "Address: %p\n", address);
        assert(lower != addressMap.end());
        assert((unsigned long) lower->first + lower->second >
                       (unsigned long) address);
        return true;
    }

    int get(int *address) {
        assert(noWild(address));
        return *address;
    }

    void *get(void **address) {
        assert(noWild(address));
        return *address;
    }
};

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "log.h"

#endif //CLANG_DEVEL_CLION_HEAP_H
