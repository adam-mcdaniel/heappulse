#pragma once

#include <interval_test.hpp>
#include <stack_csv.hpp>
#include <zlib.h>

#define MAX_COMPRESSED_SIZE 0x100000
#define MAX_PAGES (MAX_COMPRESSED_SIZE / PAGE_SIZE)

uint8_t uncompressed_buffer[MAX_COMPRESSED_SIZE], compressed_buffer[MAX_COMPRESSED_SIZE];


// Path: src/compression_test.cpp
class ObjectLivenessTest : public IntervalTest {
    CSV<16, 80000> csv;
    StackFile file;
    size_t interval_count = 0;
    std::chrono::steady_clock::time_point test_start_time;
    uint64_t test_start_time_ms;

    const char *name() const override {
        return "Object Liveness Test";
    }

    void setup() override {
        stack_debugf("Setup\n");
        test_start_time = std::chrono::steady_clock::now();
        test_start_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(test_start_time.time_since_epoch()).count();
        file = StackFile(StackString<256>("object-liveness.csv"), Mode::APPEND);
        file.clear();

        csv.title().add("Interval #");
        csv.title().add("Allocation Site");
        csv.title().add("Age (intervals)");
        csv.title().add("Age (ms since allocated)");
        csv.title().add("Is new?");
        csv.title().add("Object Address");
        csv.title().add("Time allocated (ms since start)");
        csv.title().add("Written During This Interval?");
        csv.title().add("Object Virtual Size (bytes)");
        csv.title().add("Object Physical Size (bytes)");
        csv.title().add("Object Physical Page Compression Savings (bytes)");

        csv.write(file);
        csv.clear();
        interval_count = 0;
    }

    void interval(
        const StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> &allocation_sites
    ) override {
        ++interval_count;
        size_t objects_tracked = 0;
        stack_infof("Interval %d object liveness starting...\n", interval_count);
        allocation_sites.map([&](auto return_address, AllocationSite site) {
            if (site.allocations.num_entries() == 0) return;
            stack_infof("Site 0x%x, %d allocations\n", return_address, site.allocations.num_entries());
            site.allocations.map([&](void *ptr, Allocation allocation) {
                objects_tracked++;
                stack_debugf("Object at 0x%x, age=%d, new=%d, dirty=%d\n", ptr, allocation.get_age(), allocation.is_new(), allocation.is_dirty());
                csv.new_row();
                csv.last()[0] = interval_count;
                csv.last()[1] = CSVString::format("0x%x", site.return_address);
                csv.last()[2] = allocation.get_age() + 1;
                // Age in milliseconds since allocated
                csv.last()[3] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - allocation.get_time_allocated()).count();
                csv.last()[4] = (uint64_t)allocation.is_new();
                csv.last()[5] = CSVString::format("0x%x", ptr);
                csv.last()[6] = allocation.get_time_allocated_ms() - test_start_time_ms;
                csv.last()[7] = (uint64_t)allocation.is_dirty();

                memset(uncompressed_buffer, 0, MAX_COMPRESSED_SIZE);

                allocation.protect();
                // Get the physical pages, compress them, and calculate the savings in bytes
                auto pages = allocation.physical_pages<MAX_PAGES>(false);
                size_t original_physical_size = pages.reduce<size_t>([&](auto page, auto acc) {
                    // stack_infof("Found physical page at 0x%x (virtual=0x%x, dirty=%, zero=%)\n", page.get_physical_address(), page.get_virtual_address(), page.is_dirty(), page.is_zero());
                    if (uncompressed_buffer + acc + page.size() > uncompressed_buffer + MAX_COMPRESSED_SIZE) {
                        stack_debugf("Original size %d too large, truncating to %d\n", original_physical_size, MAX_COMPRESSED_SIZE);
                        return acc;  // Stop if we're going to overflow the buffer
                    }
                    if (page.is_zero()) {
                        // stack_infof("Zero page, skipping\n");
                        // Go through bytes and see if they're all zero
                        for (size_t i = 0; i < page.size(); i++) {
                            uint8_t byte = ((uint8_t*)page.get_virtual_address())[i];
                            if (byte != 0) {
                                stack_errorf("Found non-zero byte %x in zero page\n", byte);
                                exit(1);
                            }
                        }

                        return acc;
                    }

                    memcpy(uncompressed_buffer + acc, page.get_virtual_address(), page.size());
                    return acc + page.size();
                }, 0);
                allocation.unprotect();
                
                uint64_t estimated_compressed_size = compressBound(original_physical_size);
                uint64_t compressed_size = estimated_compressed_size;

                stack_debugf("Compressing %d bytes\n", original_physical_size);

                int result = compress(compressed_buffer, &compressed_size, uncompressed_buffer, original_physical_size);

                if (result != Z_OK) {
                    stack_errorf("Compression failed with result %d\n", result);
                    exit(1);
                }

                stack_debugf("Compressed from %d bytes to %d bytes\n", original_physical_size, compressed_size);

                csv.last()[8] = allocation.size;
                csv.last()[9] = original_physical_size;
                
                // Calculate the savings
                csv.last()[10] = original_physical_size - compressed_size;
            });
            stack_infof("Site 0x%x complete\n", return_address);
        });

        csv.write(file);
        csv.clear();
        stack_infof("Interval %d complete for object liveness\n", interval_count);
        stack_infof("Tracked %d objects\n", objects_tracked);
    }

    void cleanup() override {
        stack_debugf("Cleanup\n");
        file.close();
    }
};
