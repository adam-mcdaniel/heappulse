#include <array>
#include <iostream>
#include <stdint.h>

template <typename KeyType, typename ValueType, std::size_t Size>
struct StackMap {
    struct Entry {
        KeyType key;
        ValueType value;
        bool occupied;

        Entry() : occupied(false) {}
        Entry(const KeyType& key, const ValueType& value) : key(key), value(value), occupied(true) {}
    };

    std::array<Entry, Size> hashtable;

    size_t entries = 0;

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

    void map(void (*func)(KeyType&, ValueType&));

    void print() const;

    void print_stats() const;
};