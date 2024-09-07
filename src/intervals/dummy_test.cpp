#include <stack_io.hpp>
#include <interval_test.hpp>

// An interval test that just logs some information
class DummyTest : public IntervalTest {
    size_t interval_count = 0;
    const char *name() const override { return "Dummy Test"; }
    void setup() override { stack_debugf("Dummy Test setup finished!\n"); }
    void cleanup() override { stack_debugf("Dummy Test destruction finished!\n"); }

    // A method that gets called when a huge page is allocated
    void on_huge_page_alloc(uint8_t *huge_page, size_t size) override {
        stack_infof("Huge page allocated at %p with size %d\n", huge_page, size);
    }
    // A method that gets called when a huge page is freed
    void on_huge_page_free(uint8_t *huge_page, size_t size) override {
        stack_infof("Huge page freed at %p with size %d\n", huge_page, size);
    }

    // A method that gets called when an allocation is made
    void on_alloc(const Allocation &alloc) override {
        stack_infof("Allocation made at %p with size %d\n", alloc.ptr, alloc.size);
    }
    // A method that gets called when an allocation is freed
    void on_free(const Allocation &alloc) override {
        stack_infof("Allocation freed at %p with size %d\n", alloc.ptr, alloc.size);
    }
    // A method that gets called when an allocation is accessed
    void on_access(const Allocation &alloc, bool is_write) override {
        stack_infof("Allocation accessed at %p with size %d\n", alloc.ptr, alloc.size);
    }
    // A method that gets called when an allocation is written to
    void on_write(const Allocation &alloc) override {
        stack_infof("Allocation written to at %p with size %d\n", alloc.ptr, alloc.size);
    }
    // A method that gets called when an allocation is read from
    void on_read(const Allocation &alloc) override {
        stack_infof("Allocation read from at %p with size %d\n", alloc.ptr, alloc.size);
    }

    void interval(const StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> &allocation_sites) override {
        ++interval_count;

        stack_infof("Dummy Test interval #%d finished!\n", interval_count);
    }
};



