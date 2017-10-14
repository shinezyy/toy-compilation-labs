//
// Created by diamo on 2017/10/12.
//

#ifndef CLANG_DEVEL_CLION_HEAP_H
#define CLANG_DEVEL_CLION_HEAP_H

class Heap {

    struct less {
        bool operator() (const unsigned long& lhs, const unsigned long& rhs) const {
            return lhs > rhs;
        }
    };

    std::map<unsigned long, size_t, less> addressMap;

public:
    void *Malloc(int size) {
        log(HeapState, "Allocating size: %d\n", size);
        auto *buf = (void *) malloc(static_cast<size_t>(size));
        addressMap.insert(std::make_pair((unsigned long) buf, size));
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
        auto lower = addressMap.lower_bound((unsigned long) address);
        auto upper = addressMap.upper_bound((unsigned long) address);
        log(PointerVisit || ArrayVisit, "Address: %p\n", address);

        if (upper != addressMap.end()) {
            log(PointerVisit || ArrayVisit, "Upper Address: %p\n",
                (void *)upper->first);
        }
        if (lower == addressMap.end() ||
                lower->first + lower->second <= (unsigned long) address) {
            print();
            assert(false && "Access Violation");
        }
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

    void print() {
        for (auto it = addressMap.begin(), end = addressMap.end();
                it != end; ++it) {
            log(HeapState, "Address: %p, size: %lu\n", (void *)it->first, it->second);
        }
    }
};


#endif //CLANG_DEVEL_CLION_HEAP_H
