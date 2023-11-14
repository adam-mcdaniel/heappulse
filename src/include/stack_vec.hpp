#pragma once
#include <array>
#include <stdint.h>
#include <functional>

template <typename ValueType, size_t Size>
class StackVec {
private:
    std::array<ValueType, Size> data;
    size_t elements, capacity;

public:
    void push(const ValueType& value) {
        if (elements >= capacity) {
            throw std::out_of_range("StackVec is full");
        }
        data[elements++] = value;
    }

    bool contains(const ValueType& value) const {
        for (size_t i=0; i<elements; i++) {
            if (data[i] == value) {
                return true;
            }
        }
        return false;
    }

    void sort_by(std::function<bool(const ValueType&, const ValueType&)> func) {
        std::sort(data.begin(), data.begin() + elements, func);
    }

    void sort() {
        sort_by([](const ValueType& a, const ValueType& b) {
            return a < b;
        });
    }

    ValueType pop() {
        if (elements == 0) {
            throw std::out_of_range("StackVec is empty");
        }
        return data[--elements];
    }

    ValueType& operator[](size_t index) {
        if (index >= elements) {
            throw std::out_of_range("Key not found");
        }
        return data[index];
    }

    const ValueType& operator[](size_t index) const {
        if (index >= elements) {
            throw std::out_of_range("Key not found");
        }
        return data[index];
    }

    size_t size() const {
        return elements;
    }

    size_t max_size() const {
        return capacity;
    }

    bool empty() const {
        return elements == 0;
    }

    bool full() const {
        return elements >= capacity;
    }

    void clear() {
        elements = 0;
    }

    void map(std::function<void(ValueType&)> func) {
        for (size_t i=0; i<elements; i++) {
            func(data[i]);
        }
    }

    // Map into a new StackVec with the same size
    template <typename NewValueType>
    StackVec<NewValueType, Size> map(std::function<NewValueType(const ValueType&)> func) const {
        StackVec<NewValueType, Size> new_stack;
        for (size_t i=0; i<elements; i++) {
            new_stack.push(func(data[i]));
        }
        return new_stack;
    }

    // Reduce
    template <typename T>
    T reduce(std::function<T(const ValueType&, T)> func, T initial) const {
        T result = initial;
        for (size_t i=0; i<elements; i++) {
            result = func(data[i], result);
        }
        return result;
    }

    StackVec() : elements(0), capacity(Size) {}

    // friend std::ostream& operator<<(std::ostream& os, const StackVec& stack) {
    //     os << "[";
    //     for (size_t i=0; i<stack.elements; i++) {
    //         os << stack.data[i];
    //         if (i != stack.elements - 1) {
    //             os << ", ";
    //         }
    //     }
    //     os << "]";
    //     return os;
    // }

    void print() const {
        // std::cout << *this << std::endl;
    }

    // Add two stackvecs together
    template<size_t N>
    StackVec<ValueType, Size> operator+(const StackVec<ValueType, N>& other) const {
        StackVec<ValueType, Size> result;
        for (size_t i=0; i<elements; i++) {
            result.push(data[i]);
        }
        for (size_t i=0; i<other.size(); i++) {
            result.push(other[i]);
        }
        return result;
    }
    template<size_t N>
    StackVec<ValueType, Size> operator+=(const StackVec<ValueType, N>& other) {
        for (size_t i=0; i<other.size(); i++) {
            push(other[i]);
        }
        return *this;
    }

    // Get the pointer to the underlying array
    const ValueType* array_pointer() const {
        return data.data();
    }


    StackVec<ValueType, Size> slice(size_t start, size_t end=-1ULL) const {
        StackVec<ValueType, Size> result;
        for (size_t i=start; i<end && i<capacity && i<elements; i++) {
            result.push(data[i]);
        }
        return result;
    }

    // Copy constructor
    template <size_t N>
    StackVec(const StackVec<ValueType, N>& other) {
        elements = other.size();
        capacity = other.max_size();
        for (size_t i=0; i<elements && i<capacity; i++) {
            data[i] = other[i];
        }
    }

    // Copy assignment operator
    template <size_t N>
    StackVec<ValueType, Size>& operator=(const StackVec<ValueType, N>& other) {
        elements = other.size();
        capacity = other.max_size();
        for (size_t i=0; i<elements; i++) {
            data[i] = other[i];
        }
        return *this;
    }
    
    // // Copy constructor
    // StackVec(const StackVec<ValueType, Size>& other) {
    //     elements = other.elements;
    //     capacity = other.capacity;
    //     for (size_t i=0; i<elements; i++) {
    //         data[i] = other.data[i];
    //     }
    // }

    // // Copy assignment operator
    // StackVec<ValueType, Size>& operator=(const StackVec<ValueType, Size>& other) {
    //     elements = other.elements;
    //     capacity = other.capacity;
    //     for (size_t i=0; i<elements; i++) {
    //         data[i] = other.data[i];
    //     }
    //     return *this;
    // }

    // // Move constructor
    // StackVec(StackVec<ValueType, Size>&& other) {
    //     elements = other.elements;
    //     capacity = other.capacity;
    //     for (size_t i=0; i<elements; i++) {
    //         data[i] = other.data[i];
    //     }
    // }
};