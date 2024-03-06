#pragma once

#include <interval_test.hpp>

class DummyTest : public IntervalTest {
    size_t interval_count = 0;

    const char *name() const override {
        return "Dummy Test";
    }

    void setup() override {
        stack_debugf("Setup\n");
        interval_count = 0;
    }

    void interval(
        const StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> &allocation_sites
    ) override {
        ++interval_count;
        stack_infof("Dummy test done!\n", interval_count);
    }

    void cleanup() override {
        stack_debugf("Cleanup\n");
    }
};
