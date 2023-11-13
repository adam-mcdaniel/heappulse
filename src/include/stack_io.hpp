#pragma once

#include <stack_string.hpp>
#include <stack_file.hpp>
#include <bkmalloc.h>

#define LOG_FILE "log.txt"

static StackFile log_file(StackString<256>::from(LOG_FILE), StackFile::Mode::WRITE);

template <typename... Args>
void stack_printf(const char* format, Args... args) {
    StackString<1 << 14>::format(format, args...).print();
}

void stack_printf(const char* str) {
    // StackString<1 << 14>(str).print();
    bk_printf("%s", str);
}



template <typename... Args>
void stack_fprintf(StackFile& file, const char* format, Args... args) {
    // StackString<1 << 14>::format(format, args...).write(file);
    file.write(StackString<1 << 14>::format(format, args...));
}

void stack_fprintf(StackFile& file, const char* str) {
    // StackString<1 << 14>(str).write(file);
    file.write(StackString<1 << 14>(str));
}

template <size_t Size, typename... Args>
void stack_fscanf(StackFile& file, StackString<Size> &buf, const char* format, Args... args) {
    // buf = StackString<Size>::format(format, args...);
    StackString<Size> tmp = file.read<Size>();
    // buf = StackString<Size>::format(format, args...);
    // buf.read(file);
    // file.read(buf);
}

template <size_t Size, typename... Args>
void stack_sprintf(StackString<Size> &buf, const char* format, Args... args) {
    buf = StackString<Size>::format(format, args...);
}

template <size_t Size, typename... Args>
void stack_sprintf(char *buf, const char* format, Args... args) {
    StackString<Size> buf2 = StackString<Size>::format(format, args...);
    strncpy(buf, buf2.c_str(), buf2.size());
}


void stack_logf(const char* str) {
    // StackString<1 << 14>(str).print();
    // stack_fprintf(log_file, "%s", str);
    stack_printf(str);
}


template <typename... Args>
void stack_logf(const char* format, Args... args) {
    // StackString<1 << 14>::format(format, args...).print();
    // stack_fprintf(log_file, format, args...);
    stack_printf(format, args...);
}