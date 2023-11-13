#include <interval_test.hpp>
#include <stack_csv.hpp>
#include <zlib.h>

// Path: src/compression_test.cpp
const int MAX_COMPRESSED_SIZE = 0x50000;
const int MAX_PAGES = 0x10000;
static uint8_t buffer[MAX_COMPRESSED_SIZE];

class CompressionTest : public IntervalTest {
    uint8_t compressed_data[MAX_COMPRESSED_SIZE];
    CSV<8, 50000> csv;
    StackFile file;
    size_t interval_count = 0;

    void setup() override {
        stack_logf("Setup\n");
        file = StackFile(StackString<256>("compression.csv"), StackFile::Mode::WRITE);
        // CSVRow<4> &row = 
        csv.title().add("Interval #");
        csv.title().add("Pointer");
        csv.title().add("Uncompressed Size");
        csv.title().add("Estimated Compressed Size");
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
        stack_printf("Interval %d\n", interval_count);
        double total_uncompressed_size = 0;
        double total_compressed_size = 0;
        stack_logf("copied\n");
        interval_count++;
        size_t i;
        for (i=0; i<allocations.size(); i++) {
            Allocation alloc = allocations[i];
            // if (alloc.size > MAX_COMPRESSED_SIZE / 2) {
            //     stack_logf("Skipping: Unable to compress data\n");
            //     continue;
            // }

            // alloc.log();
            // stack_logf("Protected\n");

            // for (size_t i=0; i<alloc.size; i++) {
            //     stack_logf("%d ", ((uint8_t*)alloc.ptr)[i]);
            // }
            // stack_logf("\n");
            size_t estimated_compressed_size = compressBound(alloc.size);
            size_t compressed_size = estimated_compressed_size;
            if (compressed_size > MAX_COMPRESSED_SIZE || alloc.size > MAX_COMPRESSED_SIZE || alloc.size == 0 || alloc.ptr == NULL) {
                stack_logf("Skipping: Unable to compress data\n");
                continue;
            }

            alloc.protect();
            memcpy(buffer, (const uint8_t*)alloc.ptr, alloc.size);
            alloc.unprotect();

            // stack_logf("About to compress %d bytes to %d bytes\n", alloc.size, compressed_size);
            // int result = compress(compressed_data, &compressed_size, buffer, alloc.size);
            // stack_logf("result: %d\n", result);

            csv.new_row();
            csv.last()[0] = interval_count;
            csv.last()[1] = alloc.ptr;
            csv.last()[2] = alloc.size;
            csv.last()[3] = estimated_compressed_size;
            csv.last()[4] = compressed_size;
            StackVec pages = alloc.page_info<MAX_PAGES>();
            uint64_t resident_pages = 0;
            uint64_t zero_pages = 0;
            uint64_t dirty_pages = 0;
            for (size_t j=0; j<pages.size(); j++) {
                if (pages[j].is_zero()) {
                    zero_pages++;
                }

                if (pages[j].is_resident()) {
                    resident_pages++;
                }

                if (pages[j].is_dirty()) {
                    dirty_pages++;
                }
            }
            csv.last()[5] = resident_pages;
            csv.last()[6] = zero_pages;
            csv.last()[7] = dirty_pages;

            stack_printf("Resident pages: %d\n", resident_pages);
            stack_printf("Zero pages: %d\n", zero_pages);
            stack_printf("Dirty pages: %d\n", dirty_pages);

            // if (result != Z_OK) {
            //     stack_logf("Error: Unable to compress data\n");
            //     exit(1);
            // } else {
            //     stack_logf("Compressed %d bytes to %d bytes\n", alloc.size, compressed_size);
            // }
        }
        stack_printf("Tracked %d allocations\n", i);
        csv.write(file);

        stack_printf("Interval %d done\n", interval_count);
    }
};
