#include <stack_file.hpp>
#include <array>
#include <iostream>
#include <stdint.h>
#include <functional>
#include <fcntl.h>

#pragma once

template<size_t Size>
StackFile::StackFile(StackString<Size> filename, Mode mode) : filename(filename), position(0) {
    // bk_printf("Opening file %s\n", filename.c_str());
    switch (mode) {
    case Mode::READ:
        fd = open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0777);
        break;
    case Mode::WRITE:
        fd = open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0777);
        break;
    case Mode::APPEND:
        fd = open(filename.c_str(), O_RDWR | O_CREAT | O_APPEND, 0777);
        break;
    }
    if (fd == -1) {
        throw std::runtime_error("Could not open file");
    }
}

StackFile::~StackFile() {
    close(fd);
}

// Wipe the file
void StackFile::clear() {
    close(fd);
    fd = open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0777);
    position = 0;
}

void StackFile::seek(size_t position) {
    this->position = position;
}

template <size_t Size>
StackString<Size> StackFile::read() {
    char buf[Size];

    ssize_t bytes_read = pread(fd, buf, Size, position);

    if (bytes_read == -1) {
        return StackString<Size>();
        // throw std::runtime_error("Could not read from file");
    }

    position += bytes_read;
    buf[bytes_read] = '\0';

    return StackString<Size>(buf);
}

template <size_t Size>
void StackFile::write(const StackString<Size>& data) {
    if (position == 0) {
        ssize_t bytes_written = ::write(fd, data.c_str(), data.size());

        if (bytes_written == -1) {
            throw std::runtime_error("Could not write to file");
        }

        // bk_printf("Wrote %d bytes\n", bytes_written);

        position += bytes_written;
    } else {
        ssize_t bytes_written = pwrite(fd, data.c_str(), data.size(), position);

        if (bytes_written == -1) {
            throw std::runtime_error("Could not write to file");
        }
        
        // bk_printf("Wrote %d bytes\n", bytes_written);

        position += bytes_written;
    }
}