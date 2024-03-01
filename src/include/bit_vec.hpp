#pragma once

#include <stdint.h>

template <size_t Bits>
class BitVec {
    uint8_t data[Bits / 8 + 1];
    size_t len;
    
public:
    BitVec() {
        clear();
    }

    void set(size_t index, bool value) {
        size_t byte = index / 8;
        size_t bit = index % 8;

        if (value) {
            data[byte] |= (1 << bit);
        } else {
            data[byte] &= ~(1 << bit);
        }
    }

    bool get(size_t index) const {
        size_t byte = index / 8;
        size_t bit = index % 8;

        return (data[byte] >> bit) & 1;
    }

    size_t count() const {
        size_t count = 0;
        for (size_t i=0; i<max_size(); i++) {
            if (get(i)) {
                count++;
            }
        }
        return count;
    }

    size_t count_all() const {
        size_t count = 0;
        for (size_t i=0; i<size(); i++) {
            if (get(i)) {
                count++;
            }
        }
        return count;
    }

    size_t count_not() const {
        size_t count = 0;
        for (size_t i=0; i<max_size(); i++) {
            if (!get(i)) {
                count++;
            }
        }
        return count;
    }

    size_t count_all_not() const {
        size_t count = 0;
        for (size_t i=0; i<size(); i++) {
            if (!get(i)) {
                count++;
            }
        }
        return count;
    }

    size_t size() const {
        return len;
    }

    size_t max_size() const {
        return Bits;
    }

    void clear() {
        for (size_t i=0; i<Bits / 8 + 1; i++) {
            data[i] = 0;
        }
    }

    void push(bool value) {
        set(len++, value);
    }

    bool pop() {
        return get(len--);
    }

    bool operator[](size_t index) const {
        return get(index);
    }
};