#pragma once

#include <interval_test.hpp>

class GroupTest : public IntervalTest {
public:
    GroupTest() {
        stack_debugf("Creating group test\n");
    }

    void add_test(IntervalTest *t) {
        stack_debugf("Adding test %s to group\n", t->name());
        tests.push(t);
    }

private:
    StackVec<IntervalTest*, 10> tests;
    
    const char *name() const override {
        return "Group Test";
    }

    void setup() override {
        stack_debugf("Setting up grouped tests\n");
        for (size_t i=0; i<tests.size(); i++) {
            tests[i]->setup();
        }
    }

    void interval(
        const StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> &allocation_sites
    ) override {
        for (size_t i=0; i<tests.size(); i++) {
            tests[i]->interval(allocation_sites);
        }
    }

    void on_alloc(
        const Allocation &alloc
    ) override {
        for (size_t i=0; i<tests.size(); i++) {
            tests[i]->on_alloc(alloc);
        }
    }

    void on_free(
        const Allocation &alloc
    ) override {
        for (size_t i=0; i<tests.size(); i++) {
            tests[i]->on_free(alloc);
        }
    }

    void on_access(
        const Allocation &access,
        bool is_write
    ) override {
        for (size_t i=0; i<tests.size(); i++) {
            tests[i]->on_access(access, is_write);
        }
    }

    void on_write(
        const Allocation &access
    ) override {
        for (size_t i=0; i<tests.size(); i++) {
            tests[i]->on_write(access);
        }
    }

    void on_read(
        const Allocation &access
    ) override {
        for (size_t i=0; i<tests.size(); i++) {
            tests[i]->on_read(access);
        }
    }

    void cleanup() override {
        stack_debugf("Cleaning up grouped tests\n");
        for (size_t i=0; i<tests.size(); i++) {
            tests[i]->cleanup();
        }
    }
};
