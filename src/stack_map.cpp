#include <stack_map.hpp>

#include <array>
#include <iostream>
#include <stdint.h>

template <typename KeyType, typename ValueType, size_t Size>
std::size_t StackMap<KeyType, ValueType, Size>::hash(const KeyType& key) const {
    return std::hash<KeyType>{}(key) % Size;
}


template <typename KeyType, typename ValueType, size_t Size>
StackMap<KeyType, ValueType, Size>::StackMap() : hashtable() {
    clear();
}

template <typename KeyType, typename ValueType, size_t Size>
void StackMap<KeyType, ValueType, Size>::put(const KeyType& key, const ValueType& value) {
    std::size_t index = hash(key);
    while (hashtable[index].occupied) {
        if (hashtable[index].key == key) {
            hashtable[index].value = value;  // Update value if key already exists
            return;
        }
        index = (index + 1) % Size;  // Linear probing for collision resolution
    }

    hashtable[index].key = key;
    hashtable[index].value = value;
    hashtable[index].occupied = true;
    entries++;
}

// void put(const KeyType& key, const ValueType& value) {
//     std::size_t index = hash(key);
//     while (hashtable[index].occupied) {
//         if (hashtable[index].key == key) {
//             hashtable[index].value = value;  // Update value if key already exists
//             return;
//         }
//         index = (index + 1) % Size;  // Linear probing for collision resolution
//     }

//     hashtable[index].key = key;
//     hashtable[index].value = value;
//     hashtable[index].occupied = true;
//     entries++;
// }

template <typename KeyType, typename ValueType, size_t Size>
ValueType StackMap<KeyType, ValueType, Size>::get(const KeyType& key) const {
    std::size_t index = hash(key);
    while (hashtable[index].occupied) {
        if (hashtable[index].key == key) {
            return hashtable[index].value;
        }
        index = (index + 1) % Size;  // Linear probing for collision resolution
    }
    // Key not found
    throw std::out_of_range("Key not found");
}

// ValueType get(const KeyType& key) const {
//     std::size_t index = hash(key);
//     while (hashtable[index].occupied) {
//         if (hashtable[index].key == key) {
//             return hashtable[index].value;
//         }
//         index = (index + 1) % Size;  // Linear probing for collision resolution
//     }
//     // Key not found
//     throw std::out_of_range("Key not found");
// }

template <typename KeyType, typename ValueType, size_t Size>
void StackMap<KeyType, ValueType, Size>::remove(const KeyType& key) {
    std::size_t index = hash(key);
    while (hashtable[index].occupied) {
        if (hashtable[index].key == key) {
            hashtable[index].occupied = false;
            entries--;
            return;
        }
        index = (index + 1) % Size;  // Linear probing for collision resolution
    }
    // Key not found
    throw std::out_of_range("Key not found");
}

// void remove(const KeyType& key) {
//     std::size_t index = hash(key);
//     while (hashtable[index].occupied) {
//         if (hashtable[index].key == key) {
//             hashtable[index].occupied = false;
//             entries--;
//             return;
//         }
//         index = (index + 1) % Size;  // Linear probing for collision resolution
//     }
//     // Key not found
//     throw std::out_of_range("Key not found");
// }

template <typename KeyType, typename ValueType, size_t Size>
bool StackMap<KeyType, ValueType, Size>::has(const KeyType& key) const {
    std::size_t index = hash(key);
    while (hashtable[index].occupied) {
        if (hashtable[index].key == key) {
            return true;
        }
        index = (index + 1) % Size;  // Linear probing for collision resolution
    }
    return false;
}

// bool has(const KeyType& key) const {
//     std::size_t index = hash(key);
//     while (hashtable[index].occupied) {
//         if (hashtable[index].key == key) {
//             return true;
//         }
//         index = (index + 1) % Size;  // Linear probing for collision resolution
//     }
//     return false;
// }

template <typename KeyType, typename ValueType, size_t Size>
void StackMap<KeyType, ValueType, Size>::clear() {
    for (uint64_t i=0; i<Size; i++) {
        hashtable[i].occupied = false;
    }
    entries = 0;
}

// void clear() {
//     for (uint64_t i=0; i<Size; i++) {
//         hashtable[i].occupied = false;
//     }
//     entries = 0;
// }

template <typename KeyType, typename ValueType, size_t Size>
size_t StackMap<KeyType, ValueType, Size>::size() const {
    return Size;
}

// int size() const {
//     return Size;
// }

template <typename KeyType, typename ValueType, size_t Size>
size_t StackMap<KeyType, ValueType, Size>::num_entries() const {
    return entries;
}

// bool empty() const {
//     return entries == 0;
// }

template <typename KeyType, typename ValueType, size_t Size>
bool StackMap<KeyType, ValueType, Size>::empty() const {
    return entries == 0;
}

// bool full() const {
//     return entries == Size;
// }


template <typename KeyType, typename ValueType, size_t Size>
bool StackMap<KeyType, ValueType, Size>::full() const {
    return entries >= Size;
}

// void map(void (*func)(KeyType&, ValueType&)) {
//     int j = 0;
//     for (uint64_t i=0; i<Size; i++) {
//         if (hashtable[i].occupied) {
//             func(hashtable[i].key, hashtable[i].value);
//             if (++j == entries) {
//                 break;
//             }
//         }
//     }
// }

template <typename KeyType, typename ValueType, size_t Size>
void StackMap<KeyType, ValueType, Size>::map(void (*func)(KeyType&, ValueType&)) {
    int j = 0;
    for (uint64_t i=0; i<Size; i++) {
        if (hashtable[i].occupied) {
            func(hashtable[i].key, hashtable[i].value);
            if (++j == entries) {
                break;
            }
        }
    }
}

// void print() const {
//     for (int i=0; i<Size; i++) {
//         if (hashtable[i].occupied) {
//             std::cout << hashtable[i].key << ": " << hashtable[i].value << std::endl;
//         }
//     }
// }

template <typename KeyType, typename ValueType, size_t Size>
void StackMap<KeyType, ValueType, Size>::print() const {
    for (int i=0; i<Size; i++) {
        if (hashtable[i].occupied) {
            std::cout << hashtable[i].key << ": " << hashtable[i].value << std::endl;
        }
    }
}

// void print_stats() const {
//     int num_collisions = 0;
//     int max_collisions = 0;
//     for (int i=0; i<Size; i++) {
//         if (hashtable[i].occupied) {
//             int collisions = 0;
//             int index = i;
//             while (hashtable[index].occupied) {
//                 collisions++;
//                 index = (index + 1) % Size;
//             }
//             num_collisions += collisions;
//             if (collisions > max_collisions) {
//                 max_collisions = collisions;
//             }
//         }
//     }
//     std::cout << "Number of entries: " << entries << std::endl;
//     std::cout << "Number of collisions: " << num_collisions << std::endl;
//     std::cout << "Max collisions for any given key: " << max_collisions << std::endl;
// }

template <typename KeyType, typename ValueType, size_t Size>
void StackMap<KeyType, ValueType, Size>::print_stats() const {
    size_t num_collisions = 0;
    size_t max_collisions = 0;
    for (size_t i=0; i<Size; i++) {
        if (hashtable[i].occupied) {
            size_t collisions = 0;
            size_t index = i;
            while (hashtable[index].occupied) {
                collisions++;
                index = (index + 1) % Size;
            }
            num_collisions += collisions;
            if (collisions > max_collisions) {
                max_collisions = collisions;
            }
        }
    }
    std::cout << "Number of entries: " << entries << std::endl;
    std::cout << "Number of collisions: " << num_collisions << std::endl;
    std::cout << "Max collisions for any given key: " << max_collisions << std::endl;
}