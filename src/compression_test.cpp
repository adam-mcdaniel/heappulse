#include <interval_test.hpp>
#include <stack_csv.hpp>
#include <zlib.h>

// Path: src/compression_test.cpp
const int MAX_COMPRESSED_SIZE = 0x100000;
const int MAX_PAGES = 0x10000;
static uint8_t buffer[MAX_COMPRESSED_SIZE];

class CompressionTest : public IntervalTest {
    uint8_t compressed_data[MAX_COMPRESSED_SIZE];
    CSV<12, 50000> csv;
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
        csv.title().add("Total Uncompressed Size");
        csv.title().add("Total Compressed Dirty Size");
        csv.title().add("Total Compressed Resident Size");
        csv.write(file);
        interval_count = 0;
    }

    void interval(
        const StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> &allocation_sites,
        const StackVec<Allocation, TOTAL_TRACKED_ALLOCATIONS> &allocations
    ) override {
        stack_infof("Interval %d starting...\n", ++interval_count);

        if (csv.full()) {
            stack_warnf("CSV full, omitting test interval\n");
            quit();
            return;
        }

        stack_debugf("About to iterate over %d allocation sites\n", allocation_sites.num_entries());
        // Get the allocation sites from the map
        StackVec<AllocationSite, TRACKED_ALLOCATION_SITES> sites;
        allocation_sites.values(sites);

        size_t i;
        for (i=0; i<sites.size(); i++) {
            stack_debugf("i: %d\n", i);
            if (csv.full()) {
                stack_warnf("CSV full, omitting test interval\n");
                quit();
                return;
            }

            // See where we are in the process
            if (i == sites.size() - 1) {
                stack_infof("Last allocation\n");
            } else if (i == sites.size() * 3 / 4) {
                stack_infof("Three quarters done\n");
            } else if (i == sites.size() / 2) {
                stack_infof("Half done\n");
            } else if (i == sites.size() / 4) {
                stack_infof("One quarter done\n");
            }

            // Get the allocation site
            AllocationSite site = sites[i];
            // Create a new row in the CSV
            csv.new_row();
            csv.last()[0] = interval_count;
            csv.last()[1] = site.return_address;
            stack_debugf("site.return_address: %p\n", site.return_address);

            double total_uncompressed_size = 0;
            double total_compressed_dirty_size = 0;
            double total_compressed_resident_size = 0;
            uint64_t total_resident_pages = 0;
            uint64_t total_zero_pages = 0;
            uint64_t total_dirty_pages = 0;

            stack_debugf("About to iterate over %d allocations\n", site.allocations.size());

            // Map a lambda over the allocations counting up the total uncompressed sizes,
            // total compressed dirty sizes, and total compressed resident sizes, etc.
            site.allocations.map([&](auto ptr, Allocation allocation) {
                size_t estimated_compressed_size = compressBound(allocation.size);
                size_t compressed_size = estimated_compressed_size;
                if (compressed_size > MAX_COMPRESSED_SIZE
                    || allocation.size > MAX_COMPRESSED_SIZE
                    || allocation.size == 0
                    || ptr == NULL) {
                    stack_warnf("Skipping: Unable to compress data\n");
                    return;
                }
                stack_debugf("About to compress %d bytes to %d bytes from address %p\n", allocation.size, compressed_size, ptr);
                total_uncompressed_size += allocation.size;
                allocation.protect();
                StackVec<PageInfo, TRACKED_ALLOCATIONS_PER_SITE> pages = allocation.page_info<TRACKED_ALLOCATIONS_PER_SITE>();
                // Copy to buffer
                memcpy(buffer, (const uint8_t*)ptr, allocation.size);
                allocation.unprotect();
                
                for (size_t j=0; j<pages.size(); j++) {
                    if (pages[j].is_zero()) {
                        total_zero_pages++;
                        continue;
                    }

                    size_t estimated_compressed_size = compressBound(PAGE_SIZE);
                    size_t compressed_size = estimated_compressed_size;
                    
                    // Compress the page in the buffer
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
                        total_compressed_dirty_size += compressed_size;
                    }
                }
            });

            csv.last()[2] = total_resident_pages;
            csv.last()[3] = total_zero_pages;
            csv.last()[4] = total_dirty_pages;
            csv.last()[5] = total_uncompressed_size;
            csv.last()[6] = total_compressed_dirty_size;
            csv.last()[7] = total_compressed_resident_size;
        }

        /*
        size_t i;
        for (i=0; i<allocations.size(); i++) {
            if (i == allocations.size() - 1) {
                stack_infof("Last allocation\n");
            } else if (i == allocations.size() * 3 / 4) {
                stack_infof("Three quarters done\n");
            } else if (i == allocations.size() / 2) {
                stack_infof("Half done\n");
            } else if (i == allocations.size() / 4) {
                stack_infof("One quarter done\n");
            }
            Allocation alloc = allocations[i];
            // if (alloc.size > MAX_COMPRESSED_SIZE / 2) {
            //     stack_debugf("Skipping: Unable to compress data\n");
            //     continue;
            // }

            // alloc.log();
            // stack_debugf("Protected\n");

            // for (size_t i=0; i<alloc.size; i++) {
            //     stack_debugf("%d ", ((uint8_t*)alloc.ptr)[i]);
            // }
            // stack_debugf("\n");
            size_t estimated_compressed_size = compressBound(alloc.size);
            size_t compressed_size = estimated_compressed_size;
            if (compressed_size > MAX_COMPRESSED_SIZE || alloc.size > MAX_COMPRESSED_SIZE || alloc.size == 0 || alloc.ptr == NULL) {
                stack_warnf("Skipping: Unable to compress data\n");
                continue;
            }

            alloc.protect();
            memcpy(buffer, (const uint8_t*)alloc.ptr, alloc.size);
            alloc.unprotect();

            // stack_debugf("About to compress %d bytes to %d bytes\n", alloc.size, compressed_size);
            // int result = compress(compressed_data, &compressed_size, buffer, alloc.size);
            // stack_debugf("result: %d\n", result);

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

            stack_debugf("Resident pages: %d\n", resident_pages);
            stack_debugf("Zero pages: %d\n", zero_pages);
            stack_debugf("Dirty pages: %d\n", dirty_pages);

            // if (result != Z_OK) {
            //     stack_debugf("Error: Unable to compress data\n");
            //     exit(1);
            // } else {
            //     stack_debugf("Compressed %d bytes to %d bytes\n", alloc.size, compressed_size);
            // }
        }
        stack_infof("Tracked %d allocations\n", i);
        */
        csv.write(file);

        stack_infof("Interval %d done\n", interval_count);
    }
};
