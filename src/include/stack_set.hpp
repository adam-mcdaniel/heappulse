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
        map.insert(key, nullptr);
    }

    void remove(T key) {
        map.remove(key);
    }

    bool contains(T key) {
        return map.contains(key);
    }

    size_t size() {
        return map.size();
    }

    void clear() {
        map.clear();
    }

    StackVec<T, N> items() {
        return map.keys();
    }
};