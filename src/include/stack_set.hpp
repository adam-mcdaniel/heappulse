#pragma once

#include <stack_map.hpp>
#include <stdint.h>

template<typename KeyType, size_t N>
class StackSet {
private:
    StackMap<KeyType, uint8_t, N> internal_map;
public:
    StackSet() {}

    void insert(KeyType key) {
        internal_map.put(key, 0);
    }

    void put(KeyType key) {
        internal_map.put(key, 0);
    }

    void clear() {
        internal_map.clear();
    }

    void remove(KeyType key) {
        internal_map.remove(key);
    }

    bool has(KeyType key) const {
        return internal_map.has(key);
    }

    size_t num_entries() const {
        return internal_map.num_entries();
    }

    size_t size() const {
        return internal_map.num_entries();
    }

    bool full() const {
        return internal_map.full();
    }


    void map(std::function<void(const KeyType&)> func) const {
        internal_map.map([&](const KeyType& key, uint8_t& value) {
            func(key);
        });
    }

    void map(std::function<void(KeyType&)> func) {
        internal_map.map([&](KeyType& key, uint8_t& value) {
            func(key);
        });
    }

    // Reduce
    template <typename T>
    T reduce(std::function<T(const KeyType&, T)> func, T initial) const {
        StackVec<KeyType, N> keys = items();
        T acc = initial;
        for (size_t i=0; i<keys.size(); i++) {
            acc = func(keys[i], acc);
        }
        return acc;
    }

    StackVec<KeyType, N> items() const {
        StackVec<KeyType, N> result;
        internal_map.keys(result);
        return result;
    }
};