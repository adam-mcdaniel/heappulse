#pragma once

#include <array>
#include <iostream>
#include <stdint.h>
#include <functional>
#include <stack_vec.hpp>

template <typename KeyType, typename ValueType, std::size_t Size>
class StackMap {
public:
    std::size_t hash(const KeyType& key) const {
        return std::hash<KeyType>{}(key) % Size;
    }

    StackMap() : hashtable() {
        clear();
    }

    void put(const KeyType& key, const ValueType& value) {
        std::size_t index = hash(key);
        uint64_t i = 0;
        while (hashtable[index].occupied) {
            if (hashtable[index].key == key) {
                hashtable[index].value = value;  // Update value if key already exists
                return;
            } else if (i++ > Size * 2) {
                return;  // Map is full
            }
            index = (index + 1) % Size;  // Linear probing for collision resolution
        }

        hashtable[index].key = key;
        hashtable[index].value = value;
        hashtable[index].occupied = true;
        entries++;
    }

    ValueType &get(const KeyType& key) {
        std::size_t index = hash(key);
        while (hashtable[index].occupied) {
            if (hashtable[index].key == key) {
                return hashtable[index].value;
            }
            index = (index + 1) % Size;  // Linear probing for collision resolution
        }
        // Key not found
        // throw std::out_of_range("Key not found");
        // return ValueType();
        return hashtable[0].value;
    }

    void remove(const KeyType& key) {
        std::size_t index = hash(key);
        for (size_t i=0; i<Size && hashtable[index].occupied; i++) {
            if (hashtable[index].key == key) {
                hashtable[index].occupied = false;
                entries--;
                return;
            }
            index = (index + 1) % Size;  // Linear probing for collision resolution
        }
    }

    bool has(const KeyType& key) const {
        std::size_t index = hash(key);
        while (hashtable[index].occupied) {
            if (hashtable[index].key == key) {
                return true;
            }
            index = (index + 1) % Size;  // Linear probing for collision resolution
        }
        return false;
    }

    void clear() {
        for (uint64_t i=0; i<Size; i++) {
            hashtable[i].occupied = false;
        }
        entries = 0;
    }

    size_t max_size() const {
        return Size;
    }

    size_t num_entries() const {
        return entries;
    }

    bool empty() const {
        return entries == 0;
    }

    bool full() const {
        return entries >= Size;
    }

    void map(std::function<void(const KeyType&, const ValueType&)> func) const {
        if (entries == 0) {
            return;
        }
        size_t j = 0;
        for (uint64_t i=0; i<Size && j < num_entries(); i++) {
            if (hashtable[i].occupied) {
                func(hashtable[i].key, hashtable[i].value);
                if (++j >= entries) {
                    break;
                }
            }
        }
    }

    void map(std::function<void(KeyType&, ValueType&)> func) {
        if (entries == 0) {
            return;
        }
        size_t j = 0;
        for (uint64_t i=0; i<Size && j < num_entries(); i++) {
            if (hashtable[i].occupied) {
                func(hashtable[i].key, hashtable[i].value);
                if (++j >= entries) {
                    break;
                }
            }
        }
    }

    // Map into a new StackMap with the same size
    // template <typename NewKeyType, typename NewValueType>
    // StackMap<NewKeyType, NewValueType, Size> map(std::function<std::pair<NewKeyType, NewValueType>(const KeyType&, const ValueType&)> func) const {

    // }

    // Reduce
    template <typename T>
    T reduce(std::function<T(const KeyType&, const ValueType&, T)> func, T initial) const {
        T result = initial;
        for (uint64_t i=0; i<Size; i++) {
            if (hashtable[i].occupied) {
                result = func(hashtable[i].key, hashtable[i].value, result);
            }
        }
        return result;
    }

    // void print() const;
    // void print_stats() const;

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