#pragma once

#include <interval_test.hpp>
#include <stack_csv.hpp>
#include <zlib.h>
#include <stack_map.hpp>
#include <stack_vec.hpp>

class CompressedAllocation {
public:
    CompressedAllocation() {}
private:
    size_t size;
    void *data;
};

// Path: src/compression_test.cpp
class CompressionAllocator : public IntervalTest {
public:
    StackMap<void*, CompressedAllocation, 1000> compressed_allocations;

    const char *name() const override {
        return "Compression Allocator Test";
    }

    void setup() override {}

    void interval(
        const StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> &allocation_sites
    ) override {
        // 
    }
};
