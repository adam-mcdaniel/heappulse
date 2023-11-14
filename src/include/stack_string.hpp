#pragma once

#include <stack_io.hpp>
#include <stack_vec.hpp>
#include <stdint.h>
#include <stdlib.h>
#include <cassert>

template <size_t Size>
class StackString {
private:
    StackVec<char, Size> data;

public:
    friend std::ostream& operator<<(std::ostream& os, const StackString& str) {
        for (size_t i=0; i<str.data.size(); i++) {
            os << str.data[i];
        }
        return os;
    }

    void push(char c) {
        data.push(c);
    }

    char pop() {
        return data.pop();
    }

    void operator+=(char c) {
        data.push(c);
    }

    const char *c_str() const {
        return data.array_pointer();
    }

    void print() const {
        const char *arr = data.array_pointer();
        char buf[Size + 1];
        size_t i;
        for (i=0; i<data.size() && i < Size; i++) {
            buf[i] = arr[i];
        }
        buf[i] = '\0';
        bk_printf("%s", buf);
    }

    void println() const {
        print();
        bk_printf("\n");
    }

    StackString() {}

    StackString(const char *str) {
        for (size_t i=0; str[i] != '\0' && i < Size; i++) {
            data.push(str[i]);
        }
    }

    StackString(const StackString& str) {
        for (size_t i=0; i<str.data.size() && i < Size; i++) {
            data.push(str.data[i]);
        }
    }

    StackString& operator=(const StackString& str) {
        data.clear();
        for (size_t i=0; i<str.data.size() && i < Size; i++) {
            data.push(str.data[i]);
        }
        return *this;
    }

    StackString& operator=(const char *str) {
        data.clear();
        for (size_t i=0; str[i] != '\0' && i < Size; i++) {
            data.push(str[i]);
        }
        return *this;
    }

    StackString& operator+=(const StackString& str) {
        for (size_t i=0; i<str.data.size() && i < Size; i++) {
            data.push(str.data[i]);
        }
        return *this;
    }

    StackString& operator+=(const char *str) {
        for (size_t i=0; str[i] != '\0' && i < Size; i++) {
            data.push(str[i]);
        }
        return *this;
    }

    StackString operator+(const StackString& str) const {
        StackString new_str(*this);
        new_str += str;
        return new_str;
    }

    StackString operator+(const char *str) const {
        StackString new_str(*this);
        new_str += str;
        return new_str;
    }

    bool operator==(const StackString& str) const {
        if (data.size() != str.data.size()) {
            return false;
        }
        for (size_t i=0; i<data.size() && i < Size; i++) {
            if (data[i] != str.data[i]) {
                return false;
            }
        }
        return true;
    }

    bool operator==(const char *str) const {
        size_t i;
        for (i=0; i<data.size() && str[i] != '\0' && i < Size; i++) {
            if (data[i] != str[i]) {
                return false;
            }
        }
        return i == data.size() && str[i] == '\0';
    }

    bool operator!=(const StackString& str) const {
        return !(*this == str);
    }

    bool operator!=(const char *str) const {
        return !(*this == str);
    }

    char& operator[](size_t index) {
        return data[index];
    }

    const char& operator[](size_t index) const {
        return data[index];
    }

    size_t size() const {
        return data.size();
    }

    size_t max_size() const {
        return data.max_size();
    }

    bool empty() const {
        return data.empty();
    }

    bool full() const {
        return data.full();
    }

    void clear() {
        data.clear();
    }

    bool is_space() const {
        for (size_t i=0; i<data.size() && i < Size; i++) {
            if (!isspace(data[i])) {
                return false;
            }
        }
        return true;
    }

    // Trim whitespace from the beginning of the string
    void trim_start() {
        size_t i;
        for (i=0; i<data.size() && i < Size && isspace(data[i]); i++);
        data = data.slice(i);
    }

    // Trim whitespace from the end of the string
    void trim_end() {
        size_t i;
        for (i=data.size() - 1; i < Size && isspace(data[i]); i--);
        data = data.slice(0, i + 1);
    }

    // Trim whitespace from the beginning and end of the string
    void trim() {
        trim_start();
        trim_end();
    }

    // Create string from a number
    template <typename T>
    static StackString<Size> from_number(T number, size_t radix=10) {
        if (radix == 10) {
            if constexpr (std::is_same<T, int>::value || std::is_same<T, size_t>::value) {
                char buf[Size];
                ftoa((double)number, buf, 0);
                buf[Size - 1] = '\0';
                return StackString<Size>(buf);
            }
        }

        if constexpr (std::is_same<T, double>::value) {
            assert(radix == 10);
            char buf[Size];
            ftoa(number, buf, 6);
            buf[Size - 1] = '\0';
            return StackString<Size>(buf);
        }

        StackString<Size> str;
        bool negative = false;
        if (number < 0) {
            negative = true;
            number = -number;
        }
        while (number > 0) {
            // Confirm the number is an integer at compile time
            if constexpr (std::is_same<T, int>::value || std::is_same<T, size_t>::value || std::is_same<T, uintptr_t>::value || std::is_same<T, uint64_t>::value) {
                if (number % radix < 10) {
                    str.push('0' + (number % radix));
                } else {
                    str.push('A' + (number % radix) - 10);
                }
            }
            number /= radix;
        }
        // bk_printf("%d -> %f -> %s\n", number, (double)number, str.data.array_pointer());

        if (negative) {
            str.push('-');
        }
        return str.reverse();
    }

    // Reverse the string
    StackString<Size> reverse() const {
        StackString<Size> new_str;
        for (size_t i=0; i<data.size(); i++) {
            new_str.data.push(data[data.size() - i - 1]);
        }
        return new_str;
    }

    // Convert to a number
    template <typename T>
    static T to_number(const StackString<Size>& str, size_t radix=10) {
        char buf[Size];
        size_t i;
        for (i=0; i<str.data.size() && i < Size && str.data[i] != '\0' && !isspace(str.data[i]); i++) {
            buf[i] = str.data[i];
        }
        buf[i] = '\0';

        T number = 0;
        if constexpr (std::is_same<T, double>::value) {
            return atof(buf);
        } else if constexpr (std::is_same<T, int>::value) {
            return atoi(buf);
        } else if constexpr (std::is_same<T, size_t>::value) {
            return atoi(buf);
        } else {
            return 0;
        }
        // bool negative = false;
        // size_t i;
        // // bk_printf("Scanning: ");
        // for (i=0; i<str.data.size() && isdigit(str.data[i]) || (radix > 10 && isalpha(str.data[i])); i++) {
        //     if (str.data[i] == '-') {
        //         negative = true;
        //     } else {
        //         // if constexpr (std::is_same<T, double>::value) {
        //         //     // bk_printf("Number: ");
        //         //     // StackString<Size> tmp = StackString<Size>::from_number(number);
        //         //     // // bk_printf("Number: %s\n", tmp);
        //         //     // tmp.println();
        //         // } else {
        //         //     // bk_printf("Number: %d\n", number);
        //         // }
        //         number *= radix;
        //         if (str.data[i] >= 'A' && str.data[i] <= 'Z') {
        //             number += str.data[i] - 'A' + 10;
        //         } else if (str.data[i] >= 'a' && str.data[i] <= 'z') {
        //             number += str.data[i] - 'a' + 10;
        //         } else if (str.data[i] >= '0' && str.data[i] <= '9') {
        //             number += str.data[i] - '0';
        //         }
        //     }
        // }
        // if constexpr (std::is_same<T, double>::value) {
        //     if (str.data[i] == '.') {
        //         i++;
        //         // Find the decimal part of the number
        //         T decimal = 0;
        //         T decimal_place = 1.0;
        //         for (; i<str.data.size() && isdigit(str.data[i]); i++) {
        //             decimal_place *= 10.0;
        //             decimal += (T)(str.data[i] - '0') / decimal_place;
        //         }
        //         number += decimal;
        //     }
        //     if (negative) {
        //         number = -number;
        //     }
        //     return number;
        // }

        // if (negative) {
        //     number = -number;
        // }
        // return number;
    }

    static StackString<Size> from(const StackString<Size>& str) {
        return str;
    }

    static StackString<Size> from(const char *str) {
        return StackString<Size>(str);
    }

    static StackString<Size> from(char c) {
        StackString<Size> str;
        str.push(c);
        return str;
    }

    static StackString<Size> from(int number) {
        return from_number(number);
    }

    static StackString<Size> from(size_t number) {
        return from_number(number);
    }

    static StackString<Size> from(double number) {
        return from_number(number);
    }

    static StackString<Size> from(void *ptr) {
        return from_number((uintptr_t)ptr, 16);
    }

    static StackString<Size> from(bool boolean) {
        if (boolean) {
            return from("true");
        } else {
            return from("false");
        }
    }

    template <size_t N>
    static StackString<Size> from(const StackVec<char, N>& vec) {
        StackString<Size> str;
        for (size_t i=0; i<vec.size() && i < N; i++) {
            str.push(vec[i]);
        }
        return str;
    }

    template <size_t N>
    static StackString<Size> from(const StackString<N>& str) {
        StackString<Size> result;
        for (size_t i=0; i<str.size() && i < N; i++) {
            result.push(str[i]);
        }
        return result;
    }

    // Format a string
    template <typename... Args>
    static StackString<Size> format(const char *fmt, Args ...args) {
        StackString<Size> str;
        str.format_append(fmt, args...);
        return str;
    }

    template <typename Arg>
    void scan(Arg &arg) {
        if constexpr (std::is_same<Arg, int>::value) {
            arg = to_number<int>(*this);
        } else if constexpr (std::is_same<Arg, size_t>::value) {
            arg = to_number<size_t>(*this);
        } else if constexpr (std::is_same<Arg, double>::value) {
            arg = to_number<double>(*this);
        } else if constexpr (std::is_same<Arg, void*>::value) {
            arg = (void*)to_number<uintptr_t>(*this, 16);
        } else if constexpr (std::is_same<Arg, bool>::value) {
            if (*this == "true") {
                arg = true;
            } else if (*this == "false") {
                arg = false;
            } else {
                arg = to_number<int>(*this);
            }
        } else if constexpr (std::is_same<Arg, StackString<Size>>::value) {
            arg = *this;
        } else if constexpr (std::is_same<Arg, StackVec<char, Size>>::value) {
            arg = StackVec<char, Size>();
            for (size_t i=0; i<data.size() && i < Size && data[i] != ' ' && data[i] != '\0'; i++) {
                arg.push(data[i]);
            }
        } else if constexpr (std::is_same<Arg, char*>::value || std::is_same<Arg, const char*>::value) {
            size_t i;
            for (i=0; i<data.size() && i < Size && !isspace(data[i]) && data[i] != '\0'; i++) {
                arg[i] = data[i];
            }
            arg[i] = '\0';
            // Check for char array
        } else if constexpr (std::is_array<Arg>::value && std::is_same<typename std::remove_extent<Arg>::type, char>::value) {
            size_t i;
            for (i=0; i<data.size() && i < Size && !isspace(data[i]) && data[i] != '\0'; i++) {
                arg[i] = data[i];
            }
            arg[i] = '\0';
        } else {
            arg = *this;
        }

        // Cut off the part of the string that was scanned
        size_t i;
        for (i=0; i<data.size() && i < Size && !isspace(data[i]) && data[i] != '\0'; i++) {}

        // Cut off the part of the string that was scanned
        // for (i=0; i<data.size() && i < Size; i++) {
        //     if (data[i] == ' ') {
        //         break;
        //     }
        // }
        // while (isspace(data[i]) && i<data.size() && i < Size) i++;
        data = data.slice(i + 1);
        
        if (!is_space()) {
            trim_start();
        }
    }

    // Scan many arguments
    template <typename Arg, typename... Args>
    void scan(Arg &arg, Args& ...args) {
        scan(arg);
        scan(args...);
    }
private:
    // Format a string and append it to this string
    template <typename Arg, typename... Args>
    void format_append(const char *fmt, Arg arg, Args ...args) {
        for (size_t i=0; fmt[i] != '\0' && i < Size; i++) {
            if (fmt[i] == '%') {
                if (fmt[i + 1] == '%') {
                    format_append_impl(arg);
                } else if (fmt[i + 1] == '\0') {
                    format_append_impl(arg);
                    return;
                } else if (fmt[i + 1] == 'd') {
                    // Check if the cast can be done safely
                    if constexpr (std::is_same<Arg, int>::value) {
                        format_append_impl((int)arg);
                    } else if constexpr (std::is_same<Arg, size_t>::value) {
                        format_append_impl((size_t)arg);
                    } else {
                        format_append_impl(arg);
                    }
                } else if (fmt[i + 1] == 'x' || fmt[i + 1] == 'X') {
                    if constexpr (std::is_same<Arg, int>::value) {
                        format_append_impl(StackString<Size>::from_number((size_t)arg, 16));
                    } else if constexpr (std::is_same<Arg, size_t>::value) {
                        format_append_impl(StackString<Size>::from_number((size_t)arg, 16));
                    } else {
                        format_append_impl(arg);
                    }
                } else if (fmt[i + 1] == 's') {
                    format_append_impl(StackString::from(arg));
                } else if (fmt[i + 1] == 'p') {
                    if constexpr (std::is_pointer<Arg>::value) {
                        format_append_impl((void*)arg);
                    } else {
                        format_append_impl(arg);
                    }
                } else if (fmt[i + 1] == 'f') {
                    if constexpr (std::is_same<Arg, int>::value) {
                        format_append_impl((double)arg);
                    } else if constexpr (std::is_same<Arg, size_t>::value) {
                        format_append_impl((double)arg);
                    } else {
                        format_append_impl(arg);
                    }
                } else {
                    format_append_impl(arg);
                    if constexpr (sizeof...(args) >= 1)  {
                        // Skip the format specifier and the head argument
                        format_append(fmt + i + 1, args...);
                    } else {
                        // Print the rest of the string
                        if (fmt[i + 1] != '\0') {
                            StackString<Size> str(fmt + i + 1);
                            *this += str;
                        }
                    }
                    return;
                }

                if constexpr (sizeof...(args) >= 1)  {
                    format_append(fmt + i + 2, args...);
                } else {
                    // Print the rest of the string
                    if (fmt[i + 1] != '\0' && fmt[i + 1] != '%' && fmt[i + 2] != '\0') {
                        StackString<Size> str(fmt + i + 2);
                        *this += str;
                    }
                }
                return;
            } else {
                data.push(fmt[i]);
            }
        }
    }

    // Format a string and append it to this string
    void format_append_impl(int number) {
        format_append_impl(StackString<Size>::from_number(number));
    }

    // Format a string and append it to this string
    void format_append_impl(size_t number) {
        format_append_impl(StackString<Size>::from_number(number));
    }

    // Format a string and append it to this string
    void format_append_impl(double number) {
        format_append_impl(StackString<Size>::from_number(number));
    }

    // Format a string and append it to this string
    template <size_t N>
    void format_append_impl(const StackString<N>& str) {
        for (size_t i=0; i<str.size() && i < N; i++) {
            data.push(str[i]);
        }
    }

    // Format a string and append it to this string
    void format_append_impl(char *str) {
        format_append_impl(StackString<Size>::from(str));
    }

    void format_append_impl(const char *str) {
        format_append_impl(StackString<Size>::from(str));
    }

    // Format a string and append it to this string
    void format_append_impl(void *ptr) {
        format_append_impl(StackString<Size>::from_number((uintptr_t)ptr, 16));
    }
};