#include <interval_test.hpp>
#include <stack_csv.hpp>
#include <zlib.h>

uint8_t uncompressed_buffer[MAX_COMPRESSED_SIZE], compressed_buffer[MAX_COMPRESSED_SIZE];

// Path: src/compression_test.cpp
class LivenessTest : public IntervalTest {
    CSV<16, 10000> csv;
    StackFile file;
    size_t interval_count = 0;
    std::chrono::steady_clock::time_point test_start_time;
    uint64_t test_start_time_ms;

    const char *name() const override {
        return "Liveness Test";
    }

    void setup() override {
        stack_debugf("Setup\n");
        test_start_time = std::chrono::steady_clock::now();
        test_start_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(test_start_time.time_since_epoch()).count();
        file = StackFile(StackString<256>("liveness.csv"), StackFile::Mode::WRITE);

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
        interval_count = 0;
    }

    void interval(
        const StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> &allocation_sites,
        const StackVec<Allocation, TOTAL_TRACKED_ALLOCATIONS> &allocations
    ) override {
        stack_infof("Interval %d starting...\n", ++interval_count);

        allocation_sites.map([&](auto return_address, AllocationSite site) {
            site.allocations.map([&](void *ptr, Allocation allocation) {
                csv.new_row();
                csv.last()[0] = interval_count;
                csv.last()[1] = CSVString::format("0x%x", site.return_address);
                csv.last()[2] = allocation.get_age() + 1;
                // Age in milliseconds since allocated
                csv.last()[3] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - allocation.get_time_allocated()).count();
                csv.last()[4] = (uint64_t)allocation.is_new();
                csv.last()[5] = CSVString::format("0x%x", ptr);
                csv.last()[6] = allocation.get_time_allocated_ms() - test_start_time_ms;
                csv.last()[7] = allocation.is_dirty();

                // Get the physical pages, compress them, and calculate the savings in bytes
                auto pages = allocation.physical_pages<10000>();
                size_t original_physical_size = pages.reduce<size_t>([&](auto page, auto acc) {
                    // stack_infof("Found physical page at 0x%x (virtual=0x%x, dirty=%, zero=%)\n", page.get_physical_address(), page.get_virtual_address(), page.is_dirty(), page.is_zero());
                    if (uncompressed_buffer + acc + page.size() > uncompressed_buffer + MAX_COMPRESSED_SIZE) {
                        stack_warnf("Original size %d too large, truncating to %d\n", original_physical_size, MAX_COMPRESSED_SIZE);
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
                
                uint64_t estimated_compressed_size = compressBound(original_physical_size);
                uint64_t compressed_size = estimated_compressed_size;
                
                // Compress the pages
                allocation.protect();
                stack_debugf("Protected\n");

                // Copy to buffer
                memcpy(uncompressed_buffer, (const uint8_t*)ptr, original_physical_size);
                allocation.unprotect();
                stack_debugf("Unprotected\n");

                // Print the uncompressed bytes

                #ifdef DEBUG
                stack_infof("Uncompressed bytes at %p (size=%):\n", ptr, allocation.size);
                char *ch_ptr = (char*)ptr;
                for (size_t i = 0; i < original_physical_size; i++) {
                    // stack_infof("%", (char)uncompressed_buffer[i]);
                    if (isalnum(ch_ptr[i])) {
                        bk_printf("%x ", ch_ptr[i]);
                    } else if (ch_ptr[i] == '\0') {
                        bk_printf(". ");
                    } else {
                        bk_printf("? ");
                    }
                }
                stack_infof("\n");
                #endif

                stack_debugf("Compressing %d bytes\n", original_physical_size);

                int result = compress(compressed_buffer, &compressed_size, uncompressed_buffer, original_physical_size);

                if (result != Z_OK) {
                    stack_errorf("Compression failed with result %d\n", result);
                    exit(1);
                }

                stack_infof("Compressed from %d bytes to %d bytes\n", original_physical_size, compressed_size);

                csv.last()[8] = allocation.size;
                csv.last()[9] = original_physical_size;
                
                // Calculate the savings
                csv.last()[10] = original_physical_size - compressed_size;
            });
        });

        csv.write(file);
    }
};
