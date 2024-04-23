#pragma once

#include "config.hpp"

#include <stack_string.hpp>
#include "stack_file.hpp"
#include <bkmalloc.h>

// Whether to print debug messages
// #define DEBUG true
// The log file to write to
#ifndef DEBUG
#define stack_debugf(...)
#endif

#ifdef LOG_FILE
static StackFile log_file(StackString<256>::from(LOG_FILE), Mode::WRITE);
#else
#define stack_logf(...)
#endif

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
    buf2.c_str(buf);
}


#ifdef DEBUG
void stack_debugf(const char* str);
template <typename... Args>
void stack_debugf(const char* format, Args... args);
#endif


void stack_logf(const char* str) {
    // StackString<1 << 14>(str).print();
    #ifdef DEBUG
    stack_debugf(str);
    #endif
    stack_fprintf(log_file, "%s", str);
    // stack_printf(str);
}


template <typename... Args>
void stack_logf(const char* format, Args... args) {
    // StackString<1 << 14>::format(format, args...).print();
    #ifdef DEBUG
    stack_debugf(format, args...);
    #endif
    stack_fprintf(log_file, format, args...);
    // stack_printf(format, args...);
}


static bool last_was_newline = true;

#ifdef DEBUG
void stack_debugf(const char* str);
template <typename... Args>
void stack_debugf(const char* format, Args... args);

void stack_debugf(const char* str) {
    #ifdef DEBUG
    if (last_was_newline) {
        stack_printf("[DEBUG] ");
    }
    stack_printf(str);
    if (str[strlen(str) - 1] == '\n') {
        last_was_newline = true;
    } else {
        last_was_newline = false;
    }
    #endif
}

template <typename... Args>
void stack_debugf(const char* format, Args... args) {
    #ifdef DEBUG
    if (last_was_newline) {
        stack_printf("[DEBUG] ");
    }
    stack_printf(format, args...);
    if (format[strlen(format) - 1] == '\n') {
        last_was_newline = true;
    } else {
        last_was_newline = false;
    }
    #endif
}

#endif

void stack_infof(const char* str) {
    if (last_was_newline) {
        stack_printf("[INFO] ");
    }
    stack_printf(str);
    if (str[strlen(str) - 1] == '\n') {
        last_was_newline = true;
    } else {
        last_was_newline = false;
    }
}

template <typename... Args>
void stack_infof(const char* format, Args... args) {
    if (last_was_newline) {
        stack_printf("[INFO] ");
    }
    stack_printf(format, args...);
    if (format[strlen(format) - 1] == '\n') {
        last_was_newline = true;
    } else {
        last_was_newline = false;
    }
}



void stack_warnf(const char* str) {
    if (last_was_newline) {
        stack_printf("[WARN] ");
    }
    stack_printf(str);
    if (last_was_newline) {
        stack_logf("[WARN] ");
    }
    stack_logf(str);
    if (str[strlen(str) - 1] == '\n') {
        last_was_newline = true;
    } else {
        last_was_newline = false;
    }
}

template <typename... Args>
void stack_warnf(const char* format, Args... args) {
    if (last_was_newline) {
        stack_printf("[WARN] ");
    }
    stack_printf(format, args...);
    if (last_was_newline) {
        stack_logf("[WARN] ");
    }
    stack_logf(format, args...);
    if (format[strlen(format) - 1] == '\n') {
        last_was_newline = true;
    } else {
        last_was_newline = false;
    }
}


void stack_errorf(const char* str) {
    if (last_was_newline) {
        stack_printf("[ERROR] ");
    }
    stack_printf(str);
    if (last_was_newline) {
        stack_logf("[ERROR] ");
    }
    stack_logf(str);
    if (str[strlen(str) - 1] == '\n') {
        last_was_newline = true;
    } else {
        last_was_newline = false;
    }
}

template <typename... Args>
void stack_errorf(const char* format, Args... args) {
    if (last_was_newline) {
        stack_printf("[ERROR] ");
    }
    stack_printf(format, args...);
    if (last_was_newline) {
        stack_logf("[ERROR] ");
    }
    stack_logf(format, args...);
    if (format[strlen(format) - 1] == '\n') {
        last_was_newline = true;
    } else {
        last_was_newline = false;
    }
}


#ifdef OPTIMIZE
#define stack_printf(...)
#define stack_logf(...)
#define stack_infof(...)
#define stack_warnf(...)
#define stack_debugf(...)
#endif
