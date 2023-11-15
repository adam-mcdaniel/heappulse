#include <interval_test.hpp>
#include <stack_csv.hpp>
#include <zlib.h>

// Path: src/compression_test.cpp
const int MAX_COMPRESSED_SIZE = 0x100000;
const int MAX_PAGES = 0x10000;
static uint8_t buffer[MAX_COMPRESSED_SIZE];

class CompressionTest : public IntervalTest {
    uint8_t compressed_data[MAX_COMPRESSED_SIZE];
    CSV<14, 10000> csv;
    StackFile file;
    size_t interval_count = 0;

    const char *name() const override {
        return "Compression Test";
    }

    void setup() override {
        stack_debugf("Setup\n");
        file = StackFile(StackString<256>("compression.csv"), StackFile::Mode::WRITE);
        // CSVRow<4> &row = 
        csv.title().add("Interval #");
        csv.title().add("Allocation Site");
        csv.title().add("Resident Pages");
        csv.title().add("Zero Pages");
        csv.title().add("Dirty Pages");
        csv.title().add("Non-Zero Byte Count");
        csv.title().add("Zero Byte Count");
        csv.title().add("Total Uncompressed Resident Size");
        csv.title().add("Total Uncompressed Dirty Size");
        csv.title().add("Total Compressed Resident Size");
        csv.title().add("Total Compressed Dirty Size");
        csv.write(file);
        interval_count = 0;
    }

    void interval(
        const StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> &allocation_sites,
        const StackVec<Allocation, TOTAL_TRACKED_ALLOCATIONS> &allocations
    ) override {
        stack_infof("Interval %d starting...\n", ++interval_count);

        // if (csv.full()) {
        //     stack_warnf("CSV full, omitting test interval\n");
        //     quit();
        //     return;
        // }

        stack_debugf("About to iterate over %d allocation sites\n", allocation_sites.num_entries());
        uint64_t i = 0;

        allocation_sites.map([&](auto return_address, AllocationSite site) {
            stack_debugf("site.return_address: %p\n", site.return_address);

            double total_uncompressed_resident_size = 0;
            double total_uncompressed_dirty_size = 0;
            double total_compressed_dirty_size = 0;
            double total_compressed_resident_size = 0;
            uint64_t total_resident_pages = 0;
            uint64_t total_zero_pages = 0;
            uint64_t total_dirty_pages = 0;
            double total_zero_bytes = 0;
            double total_non_zero_bytes = 0;
            i++;
            if (i == allocation_sites.size() - 1) {
                stack_infof("Last one\n");
            } else if (i == allocation_sites.size() * 3 / 4) {
                stack_infof("3/4 done\n");
            } else if (i == allocation_sites.size() / 2) {
                stack_infof("1/2 done\n");
            } else if (i == allocation_sites.size() / 4) {
                stack_infof("1/4 done\n");
            } else if (i == 1) {
                stack_infof("First one\n");
            }
            site.allocations.map([&](auto ptr, Allocation allocation) {
                if (ptr == NULL) {
                    stack_warnf("Skipping NULL allocation\n");
                    return;
                } else if (allocation.size == 0) {
                    stack_warnf("Skipping allocation of size 0\n");
                    return;
                }
                // stack_debugf("About to compress %d bytes to %d bytes from address %p\n", allocation.size, compressed_size, ptr);
                total_uncompressed_resident_size += allocation.size;
                size_t size = allocation.size;
                if (size > MAX_COMPRESSED_SIZE) {
                    size = MAX_COMPRESSED_SIZE;
                    stack_warnf("Truncating allocation size of %d to %d bytes\n", (uint64_t)allocation.size, (uint64_t)size);
                }

                allocation.protect();
                stack_debugf("Protected\n");
                StackVec<PageInfo, TRACKED_ALLOCATIONS_PER_SITE> pages = allocation.page_info<TRACKED_ALLOCATIONS_PER_SITE>();
                // Copy to buffer
                stack_debugf("Unprotected\n");

                memcpy(buffer, (const uint8_t*)ptr, size);
                allocation.unprotect();
                // Count the zero and non-zero bytes
                for (size_t i=0; i<size; i++) {
                    if (buffer[i] == 0) {
                        total_zero_bytes++;
                    } else {
                        total_non_zero_bytes++;
                    }
                }

                for (size_t j=0; j<pages.size(); j++) {
                    stack_debugf("j: %d\n", j);
                    if (pages[j].is_zero()) {
                        stack_debugf("Zero page\n");
                        total_zero_pages++;
                        continue;
                    }

                    uint64_t estimated_compressed_size = compressBound(PAGE_SIZE);
                    uint64_t compressed_size = estimated_compressed_size;
                    
                    // Compress the page in the buffer
                    // stack_debugf("About to compress %d bytes to %d bytes from address %p\n", PAGE_SIZE, compressed_size, buffer + j * PAGE_SIZE);
                    stack_debugf("About to compress %d bytes!\n", compressed_size);
                    // Dont read past end of buffer
                    if (j * PAGE_SIZE + PAGE_SIZE > size) {
                        stack_warnf("Truncating page size of %d to %d bytes\n", (uint64_t)PAGE_SIZE, (uint64_t)(size - j * PAGE_SIZE));
                        compressed_size = compress(compressed_data, &compressed_size, buffer + j * PAGE_SIZE, size - j * PAGE_SIZE);
                    } else {
                        compressed_size = compress(compressed_data, &compressed_size, buffer + j * PAGE_SIZE, PAGE_SIZE);
                    }
                    int result = compress(compressed_data, &compressed_size, buffer + j * PAGE_SIZE, PAGE_SIZE);

                    if (result != Z_OK) {
                        stack_warnf("Error: Unable to compress data\n");
                        continue;
                    }

                    if (pages[j].is_resident()) {
                        total_resident_pages++;
                        total_compressed_resident_size += compressed_size;
                    }

                    if (pages[j].is_dirty()) {
                        total_dirty_pages++;
                        total_uncompressed_dirty_size += PAGE_SIZE;
                        total_compressed_dirty_size += compressed_size;
                    }
                }
            });
            if (total_uncompressed_dirty_size > total_uncompressed_resident_size) {
                total_uncompressed_dirty_size = total_uncompressed_resident_size;
            }

            if (total_uncompressed_resident_size == 0) {
                stack_warnf("Skipping: No uncompressed data\n");
                return;
            }

            csv.new_row();
            csv.last()[0] = interval_count;
            csv.last()[1] = StackString<CSV_STR_SIZE>::format("%X", site.return_address);
            csv.last()[2] = total_resident_pages;
            csv.last()[3] = total_zero_pages;
            csv.last()[4] = total_dirty_pages;
            csv.last()[5] = total_non_zero_bytes;
            csv.last()[6] = total_zero_bytes;
            csv.last()[7] = total_uncompressed_resident_size;
            csv.last()[8] = total_uncompressed_dirty_size;
            csv.last()[9] = total_compressed_resident_size;
            csv.last()[10] = total_compressed_dirty_size;
        });

        csv.write(file);

        stack_infof("Interval %d done\n", interval_count);
    }
};
