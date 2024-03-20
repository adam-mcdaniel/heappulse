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
        if (full() && !has(key)) {
            return;
        }
        // get(key) = value;
        std::size_t index = hash(key);
        uint64_t i = 0;
        while (hashtable[index].occupied) {
            if (hashtable[index].key == key) {
                hashtable[index].value = value;  // Update value if key already exists
                return;
            } else if (i++ > Size) {
                return;  // Map is full
            }

            // index = std::hash<size_t>{}(index) % Size;
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

            // index = std::hash<size_t>{}(index) % Size;
            index = (index + 1) % Size;  // Linear probing for collision resolution
        }

        if (full()) {
            return hashtable[0].value;  // Return first value if map is full
        }
        // Key not found
        // throw std::out_of_range("Key not found");
        hashtable[index].key = key;
        hashtable[index].occupied = true;
        entries++;
        return hashtable[index].value;
    }

    ValueType &operator[](const KeyType& key) {
        /*
        std::size_t index = hash(key);
        while (hashtable[index].occupied) {
            if (hashtable[index].key == key) {
                return hashtable[index].value;
            }
            
            // index = std::hash<size_t>{}(index) % Size;
            index = (index + 1) % Size;  // Linear probing for collision resolution
        }

        hashtable[index].key = key;
        hashtable[index].occupied = true;
        entries++;
        return hashtable[index].value;
        */
        return get(key);
    }

    void remove(const KeyType& key) {
        std::size_t index = hash(key);
        for (size_t i=0; i<Size && hashtable[index].occupied; i++) {
            if (hashtable[index].key == key) {
                hashtable[index].occupied = false;
                entries--;
                return;
            }
            // index = std::hash<size_t>{}(index) % Size;
            index = (index + 1) % Size;  // Linear probing for collision resolution
        }
    }

    bool has(const KeyType& key) const {
        std::size_t index = hash(key);
        while (hashtable[index].occupied) {
            if (hashtable[index].key == key) {
                return true;
            }
            // index = std::hash<size_t>{}(index) % Size;
            index = (index + 1) % Size;  // Linear probing for collision resolution
        }
        return false;
    }

    void clear() {
        // for (uint64_t i=0; i<Size; i++) {
        //     hashtable[i].occupied = false;
        // }
        memset((uint8_t*)hashtable.data(), 0, sizeof(hashtable));
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
        for (uint64_t i=0; i<Size && j<num_entries(); i++) {
            if (hashtable[i].occupied) {
                func(hashtable[i].key, hashtable[i].value);
                j++;
            }
        }
    }

    void map(std::function<void(KeyType&, ValueType&)> func) {
        if (entries == 0) {
            return;
        }
        size_t j = 0;
        for (uint64_t i=0; i<Size && j<num_entries(); i++) {
            if (hashtable[i].occupied) {
                func(hashtable[i].key, hashtable[i].value);
                j++;
            }
        }
    }

    // Reduce
    template <typename T>
    T reduce(std::function<T(const KeyType&, const ValueType&, T)> func, T initial) const {
        T result = initial;
        uint64_t j = 0;
        for (uint64_t i=0; i<Size && j<num_entries(); i++) {
            if (hashtable[i].occupied) {
                result = func(hashtable[i].key, hashtable[i].value, result);
                j++;
            }
        }
        return result;
    }

    // Reduce
    template <typename T>
    T reduce(std::function<T(const KeyType&, ValueType&, T)> func, T initial) {
        T result = initial;
        uint64_t j = 0;
        for (uint64_t i=0; i<Size && j<num_entries(); i++) {
            if (hashtable[i].occupied) {
                result = func(hashtable[i].key, hashtable[i].value, result);
                j++;
            }
        }
        return result;
    }

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

    template <size_t N>
    void keys(StackVec<KeyType, N> &keys) const {
        keys.clear();
        uint64_t j = 0;
        for (size_t i = 0; i < Size && j < num_entries(); i++) {
            if (hashtable[i].occupied) {
                keys.push(hashtable[i].key);
                j++;
            }
        }
    }

    template <size_t N>
    void values(StackVec<ValueType, N> &values) const {
        values.clear();
        uint64_t j = 0;
        for (size_t i = 0; i < Size && j < num_entries(); i++) {
            if (hashtable[i].occupied) {
                values.push(hashtable[i].value);
                j++;
            }
        }
    }
private:
    std::array<Entry, Size> hashtable;

    size_t entries = 0;
};