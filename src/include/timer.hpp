#pragma once

class Timer {
private:
    std::chrono::steady_clock::time_point start_time;
public:
    Timer() : start_time(std::chrono::steady_clock::now()) { }

    ~Timer() {}

    void start() {
        this->reset();
    }
    
    void reset() {
        this->start_time = std::chrono::steady_clock::now();
    }

    bool has_elapsed(uint64_t milliseconds) const {
        return this->elapsed_milliseconds() >= milliseconds;
    }

    uint64_t elapsed_seconds() const {
        return this->elapsed_milliseconds() / 1000;
    }

    uint64_t elapsed_milliseconds() const {
        return this->elapsed_microseconds() / 1000;
    }

    uint64_t elapsed_microseconds() const {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - this->start_time).count();
        return duration;
    }
};

