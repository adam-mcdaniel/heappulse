#include <interval_test.hpp>
#include <stack_csv.hpp>
#include <zlib.h>

// Path: src/compression_test.cpp
const int MAX_COMPRESSED_SIZE = 0x1000000;
const int MAX_PAGES = MAX_COMPRESSED_SIZE / 0x1000;

class CompressionTest : public IntervalTest {
    uint8_t compressed_data[MAX_COMPRESSED_SIZE];
    CSV<7, 500000> csv;
    StackFile file;
    size_t interval_count = 0;

    void setup() override {
        stack_logf("Setup\n");
        file = StackFile(StackString<256>("compression.csv"), StackFile::Mode::WRITE);
        // CSVRow<4> &row = 
        csv.title().add("Interval #");
        csv.title().add("Pointer");
        csv.title().add("Uncompressed Size");
        csv.title().add("Compressed Size");
        csv.title().add("Resident Pages");
        csv.title().add("Zero Pages");
        csv.title().add("Dirty Pages");
        csv.write(file);
        interval_count = 0;
    }

    void interval(
        const StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> &allocation_sites,
        const StackVec<Allocation, TOTAL_TRACKED_ALLOCATIONS> &allocations
    ) override {
        double total_uncompressed_size = 0;
        double total_compressed_size = 0;
        stack_logf("copied\n");
        interval_count++;

        for (size_t i=0; i<allocations.size(); i++) {
            Allocation alloc = allocations[i];
            alloc.protect();

            alloc.log();
            stack_logf("Protected\n");

            for (size_t i=0; i<alloc.size; i++) {
                stack_logf("%d ", ((uint8_t*)alloc.ptr)[i]);
            }
            stack_logf("\n");
            size_t compressed_size = compressBound(alloc.size);

            if (alloc.size > MAX_COMPRESSED_SIZE) {
                stack_logf("Skipping: Unable to compress data\n");
                continue;
            }

            uint8_t buffer[MAX_COMPRESSED_SIZE];
            memcpy(buffer, (const uint8_t*)alloc.ptr, alloc.size);
            alloc.unprotect();

            stack_logf("About to compress %d bytes to %d bytes\n", alloc.size, compressed_size);
            if (compressed_size > MAX_COMPRESSED_SIZE || alloc.size > MAX_COMPRESSED_SIZE || alloc.size == 0 || alloc.ptr == NULL) {
                stack_logf("Skipping: Unable to compress data\n");
                continue;
            }
            int result = compress(compressed_data, &compressed_size, buffer, alloc.size);
            stack_logf("result: %d\n", result);

            csv.new_row();
            csv.last()[0] = interval_count;
            csv.last()[1] = alloc.ptr;
            csv.last()[2] = alloc.size;
            csv.last()[3] = compressed_size;
            StackVec pages = alloc.page_info<MAX_PAGES>();
            uint64_t resident_pages = 0;
            uint64_t zero_pages = 0;
            uint64_t dirty_pages = 0;
            for (size_t i=0; i<pages.size(); i++) {
                if (pages[i].is_zero()) {
                    zero_pages++;
                }

                if (pages[i].is_resident()) {
                    resident_pages++;
                }

                if (pages[i].is_dirty()) {
                    dirty_pages++;
                }
            }
            csv.last()[4] = resident_pages;
            csv.last()[5] = zero_pages;
            csv.last()[6] = dirty_pages;

            if (result != Z_OK) {
                stack_logf("Error: Unable to compress data\n");
                exit(1);
            } else {
                stack_logf("Compressed %d bytes to %d bytes\n", alloc.size, compressed_size);
            }
        }
        csv.write(file);

        stack_logf("Interval %d done\n", interval_count);
    }
};
