#pragma once
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdexcept>
// A file object that only uses stack memory
class StackFile;
enum class Mode {
    READ = 1,
    WRITE = 2,
    APPEND = 3
};
#include "stack_io.hpp"
#include "stack_string.hpp"



class StackFile {
private:
    // The file descriptor
    int fd;

    // The name of the file
    char filename[256];

    // The position
    size_t position;

    // The mode
    Mode mode;
public:
    StackFile() : fd(-1), position(0) {
        memset(filename, 0, 256);
    }

    Mode get_mode() const {
        return mode;
    }

    const char *get_filename() const {
        return filename;
    }

    // Open a file
    template<size_t Size>
    StackFile(StackString<Size> name, Mode mode) {
        // stack_debugf("Opening file %s\n", filename.c_str());
        // char buf[Size + 1];
        size_t i;
        for (i=0; i<name.size() && i < Size; i++) {
            filename[i] = name.c_str()[i];
        }
        filename[i] = '\0';

        switch (mode) {
        case Mode::READ:
            fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0777);
            break;
        case Mode::WRITE:
            fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0777);
            break;
        case Mode::APPEND:
            fd = open(filename, O_RDWR | O_CREAT, 0777);
            seek(size());
            break;
        }

        this->mode = mode;
        this->position = 0;

        if (fd == -1) {
            throw std::runtime_error("Could not open file");
        } else {
            // bk_printf("Opening file %s with descriptor %d\n", filename, fd);
            // stack_debugf("Opened file %s with descriptor %d\n", buf, fd);
        }
    }

    // Get the descriptor
    int get_descriptor() const {
        return fd;
    }

    // // Close the file
    // ~StackFile() {
    //     close(fd);
    // }
    // Wipe the file
    void clear() {
        // Wipe all the contents of the file
        ::close(fd);
        // char buf[filename.max_size() + 1];
        // size_t i;
        // for (i=0; i<filename.size() && i < filename.max_size(); i++) {
        //     buf[i] = filename.c_str()[i];
        // }
        // buf[i] = '\0';
        fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0777);
    }

    void close() {
        ::close(fd);
    }

    // Seek to a position
    void seek(size_t position) {
        lseek(fd, position, SEEK_SET);
        this->position = position;
    }

    // Read from the file
    template <size_t Size>
    StackString<Size> read() {
        StackString<Size> result;
        char buf[Size + 1];
        ssize_t bytes = read(fd, buf, Size);
        if (bytes == -1) {
            throw std::runtime_error("Could not read from file");
        }
        buf[bytes] = '\0';
        result = buf;
        position += bytes;
        return result;
    }

    // Write to the file
    template <size_t Size>
    void write(const StackString<Size>& data) {
        ssize_t bytes = ::write(fd, data.c_str(), data.size());
        if (bytes == -1) {
            throw std::runtime_error("Could not write to file");
        }
        position += bytes;
    }

    // Get the size of the file
    size_t size() const {
        struct stat st;
        fstat(fd, &st);
        return st.st_size;
    }
};