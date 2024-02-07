#include <interval_test.hpp>
#include <stack_csv.hpp>
#include <zlib.h>


// Path: src/compression_test.cpp
class PageLivenessTest : public IntervalTest {
    CSV<16, 10000> csv;
    StackFile file;
    size_t interval_count = 0;
    std::chrono::steady_clock::time_point test_start_time;
    uint64_t test_start_time_ms;

    const char *name() const override {
        return "Page Liveness Test";
    }

    void setup() override {
        stack_debugf("Setup\n");
        test_start_time = std::chrono::steady_clock::now();
        test_start_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(test_start_time.time_since_epoch()).count();
        file = StackFile(StackString<256>("page-liveness.csv"), Mode::APPEND);
        file.clear();

        csv.title().add("Interval #");
        csv.title().add("Allocation Site");
        csv.title().add("Age (intervals)");
        csv.title().add("Age (ms since allocated)");
        csv.title().add("Is new?");
        csv.title().add("Object Address");
        csv.title().add("Virtual Page Address");
        csv.title().add("Physical Page Address");
        csv.title().add("Time allocated (ms since start)");
        csv.title().add("Written During This Interval?");
        csv.title().add("Compression Savings (bytes)");

        csv.write(file);
        csv.clear();
        interval_count = 0;
    }

    void interval(
        const StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> &allocation_sites,
        const StackVec<Allocation, TOTAL_TRACKED_ALLOCATIONS> &allocations
    ) override {
        stack_infof("Interval %d page liveness starting...\n", ++interval_count);
        allocation_sites.map([&](auto return_address, AllocationSite site) {
            site.allocations.map([&](void *ptr, Allocation allocation) {
                auto pages = allocation.physical_pages<10000>();
                pages.map([&](auto page) {
                    if (page.is_zero()) {
                        return;
                    }
                    csv.new_row();
                    csv.last()[0] = interval_count;
                    csv.last()[1] = CSVString::format("0x%x", site.return_address);
                    csv.last()[2] = allocation.get_age() + 1;
                    // Age in milliseconds since allocated
                    csv.last()[3] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - allocation.get_time_allocated()).count();
                    csv.last()[4] = (uint64_t)allocation.is_new();
                    csv.last()[5] = CSVString::format("0x%x", ptr);
                    csv.last()[6] = CSVString::format("0x%x", page.get_virtual_address());
                    csv.last()[7] = CSVString::format("0x%x", page.get_physical_address());
                    csv.last()[8] = allocation.get_time_allocated_ms() - test_start_time_ms;
                    csv.last()[9] = (uint64_t)allocation.is_dirty();
                    // Compress the page
                    size_t original_physical_size = page.size();
                    uint64_t estimated_compressed_size = compressBound(original_physical_size);
                    uint64_t compressed_size = estimated_compressed_size;
                    int8_t compressed_buffer[estimated_compressed_size] = {0};
                    allocation.protect();
                    int result = compress((Bytef*)compressed_buffer, &compressed_size, (const uint8_t*)page.get_virtual_address(), original_physical_size);
                    allocation.unprotect();

                    if (result != Z_OK) {
                        stack_errorf("Compression failed with result %d\n", result);
                        exit(1);
                    }
                    csv.last()[10] = original_physical_size - compressed_size;
                });
            });
        });

        csv.write(file);
        csv.clear();
        stack_infof("Interval %d complete for page liveness\n", interval_count);
    }
};
