#pragma once

#include <stack_map.hpp>
#include <stdint.h>

template<typename T, size_t N>
class StackSet {
private:
    StackMap<T, uint8_t, N> map;
public:
    StackSet() {}

    void insert(T key) {
        map.put(key, 0);
    }

    void put(T key) {
        map.put(key, 0);
    }

    void clear() {
        map.clear();
    }

    void remove(T key) {
        map.remove(key);
    }

    bool has(T key) const {
        return map.has(key);
    }

    size_t num_entries() const {
        return map.num_entries();
    }

    size_t size() const {
        return map.num_entries();
    }

    StackVec<T, N> items() const {
        StackVec<T, N> result;
        map.keys(result);
        return result;
    }
};