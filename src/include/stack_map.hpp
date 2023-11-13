#pragma once

#include <array>
#include <iostream>
#include <stdint.h>
#include <functional>
#include <stack_vec.hpp>

template <typename KeyType, typename ValueType, std::size_t Size>
class StackMap {
public:
    std::size_t hash(const KeyType& key) const;

    StackMap();

    void put(const KeyType& key, const ValueType& value);

    ValueType get(const KeyType& key) const;

    void remove(const KeyType& key);

    bool has(const KeyType& key) const;

    void clear();

    size_t size() const;

    size_t num_entries() const;

    bool empty() const;

    bool full() const;

    void map(std::function<void(const KeyType&, const ValueType&)> func) const;
    void map(std::function<void(KeyType&, ValueType&)> func);

    // Map into a new StackMap with the same size
    template <typename NewKeyType, typename NewValueType>
    StackMap<NewKeyType, NewValueType, Size> map(std::function<std::pair<NewKeyType, NewValueType>(const KeyType&, const ValueType&)> func) const;

    // Reduce
    template <typename T>
    T reduce(std::function<T(const KeyType&, const ValueType&, T)> func, T initial) const;

    void print() const;
    void print_stats() const;

    struct Entry {
        KeyType key;
        ValueType value;
        bool occupied;

        Entry() : occupied(false) {}
        Entry(const KeyType& key, const ValueType& value) : key(key), value(value), occupied(true) {}
    };
    Entry &operator[](size_t index) {
        return hashtable[index];
    }

    const Entry &operator[](size_t index) const {
        return hashtable[index];
    }
    Entry &nth_entry(size_t index) {
        return hashtable[index];
    }

    const Entry &nth_entry(size_t index) const {
        return hashtable[index];
    }

    ValueType &operator[](const KeyType& key) {
        std::size_t index = hash(key);
        while (hashtable[index].occupied) {
            if (hashtable[index].key == key) {
                return hashtable[index].value;
            }
            index = (index + 1) % Size;  // Linear probing for collision resolution
        }

        hashtable[index].key = key;
        hashtable[index].occupied = true;
        entries++;
        return hashtable[index].value;
    }

    template <size_t N>
    void keys(StackVec<KeyType, N> &keys) const {
        for (size_t i = 0; i < Size; i++) {
            if (hashtable[i].occupied) {
                keys.push(hashtable[i].key);
            }
        }
    }

    template <size_t N>
    void values(StackVec<ValueType, N> &values) const {
        for (size_t i = 0; i < Size; i++) {
            if (hashtable[i].occupied) {
                values.push(hashtable[i].value);
            }
        }
    }
private:
    std::array<Entry, Size> hashtable;

    size_t entries = 0;
};