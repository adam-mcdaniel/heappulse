#pragma once
#include "stack_string.hpp"

// A file object that only uses stack memory
class StackFile {
private:
    // The file descriptor
    int fd;

    // The name of the file
    StackString<256> filename;

    // The position
    size_t position;
public:
    enum class Mode {
        READ,
        WRITE,
        APPEND
    };

    StackFile() : fd(-1), filename(), position(0) {}

    // Open a file
    template<size_t Size>
    StackFile(StackString<Size> filename, Mode mode);

    // Close the file
    ~StackFile();
    // Wipe the file
    void clear();

    // Seek to a position
    void seek(size_t position);

    // Read from the file
    template <size_t Size>
    StackString<Size> read();

    // Write to the file
    template <size_t Size>
    void write(const StackString<Size>& data);
};



