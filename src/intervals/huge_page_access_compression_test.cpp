#pragma once

#include <config.hpp>
#include <interval_test.hpp>
#include <stack_csv.hpp>
#include <compressor.hpp>

// #define MAX_COMPRESSED_SIZE 0x100000

struct HugePage {
    uint8_t *address;
    size_t size;
    uint64_t age;

    bool written_to = false;
    bool read_from = false;
    bool accessed = false;

    HugePage() : address(nullptr), size(0), age(0) {}
    HugePage(uint8_t *address, size_t size) : address(address), size(size), age(0) {}

    bool contains(const Allocation &allocation) const {
        return address <= allocation.ptr && (uintptr_t)address + size >= (uintptr_t)allocation.ptr + allocation.size;
    }

    void tick_age() {
        age++;
    }

    void clear() {
        written_to = false;
        read_from = false;
        accessed = false;
    }

    bool operator==(const HugePage &other) const {
        return address == other.address;
    }
};

// Define hash function for HugePage
namespace std {
    template<>
    struct hash<HugePage> {
        size_t operator()(const HugePage &page) const {
            return std::hash<uint8_t*>()(page.address);
        }
    };
}

class HugePageAccessCompressionTest : public IntervalTest {
public:
    HugePageAccessCompressionTest(CompressionType type) : IntervalTest() {
        stack_debugf("Creating huge page access compression test\n");
        init_compression();
        compression_type = type;
    }

    HugePageAccessCompressionTest() : HugePageAccessCompressionTest(DEFAULT_COMPRESSION_TYPE) {}
private:
    CSV<64, 80000> csv;
    StackFile file;
    size_t interval_count = 0;
    std::chrono::steady_clock::time_point test_start_time;
    int64_t test_start_time_ms;
    
    int64_t memory_allocated_since_last_interval = 0;
    int64_t memory_freed_since_last_interval = 0;

    int64_t total_memory_allocated = 0;
    int64_t total_memory_freed = 0;

    int64_t total_objects_live = 0;
    int64_t total_bytes_live = 0;
    
    int64_t objects_written_to_this_interval = 0;
    int64_t objects_read_from_this_interval = 0;

    int64_t bytes_written_to_this_interval = 0;
    int64_t bytes_read_from_this_interval = 0;

    StackSet<HugePage, 10000> huge_page_liveset;

    StackSet<Allocation, 50000> accessed_this_interval,
                                write_accessed_this_interval,
                                read_accessed_this_interval,
                                live_this_interval;
    CompressionType compression_type;

    const char *name() const override {
        return "Huge-Page Access Compression Test";
    }

    void setup() override {
        stack_debugf("Setup\n");
        test_start_time = std::chrono::steady_clock::now();
        test_start_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(test_start_time.time_since_epoch()).count();
        // file = StackFile(StackString<256>("access-compression.csv"), Mode::APPEND);
        file = StackFile(format<256>("huge-page-access-%s-compression.csv", compression_to_string(compression_type)), Mode::APPEND);
        file.clear();

        csv.title().add("Interval #");
        csv.title().add("Huge Page Address");
        csv.title().add("Age (intervals)");
        csv.title().add("Page Size (bytes)");
        csv.title().add("Compressed Size (bytes)");
        csv.title().add("New?");
        csv.title().add("Written?");
        csv.title().add("Read?");
        csv.title().add("Unaccessed?");
        csv.title().add("Compression Ratio (compressed/uncompressed)");
        csv.title().add("Live Pages");


        csv.write(file);
        csv.clear();

        interval_count = 0;
    }

    void summary() {
        /*
        // stack_infof("Total allocations: %d\n", total_allocations);
        // stack_infof("Total frees: %d\n", total_frees);
        int64_t objects_accessed_this_interval = accessed_this_interval.size();
        int64_t bytes_accessed_this_interval = accessed_this_interval.reduce<int64_t>([](const Allocation &alloc, int64_t acc) {
            return acc + alloc.size;
        }, 0);

        int64_t objects_unaccessed_this_interval = total_objects_live - objects_accessed_this_interval;
        int64_t bytes_unaccessed_this_interval = total_bytes_live - bytes_accessed_this_interval;
        
        stack_infof("Total objects live: %d\n", total_objects_live);
        stack_infof("Objects accessed this interval: %d\n", objects_accessed_this_interval);
        stack_infof("Objects written to this interval: %d\n", objects_written_to_this_interval);
        stack_infof("Objects read from this interval: %d\n", objects_read_from_this_interval);
        stack_infof("Objects unaccessed this interval: %d\n", objects_unaccessed_this_interval);
        stack_infof("Bytes accessed this interval: %d\n", bytes_accessed_this_interval);
        stack_infof("Bytes written to this interval: %d\n", bytes_written_to_this_interval);
        stack_infof("Bytes read from this interval: %d\n", bytes_read_from_this_interval);
        stack_infof("Bytes unaccessed this interval: %d\n", bytes_unaccessed_this_interval);
        stack_infof("Memory allocated since last interval: %d\n", memory_allocated_since_last_interval);
        stack_infof("Memory freed since last interval: %d\n", memory_freed_since_last_interval);
        stack_infof("Total memory allocated: %d\n", total_memory_allocated);
        stack_infof("Total memory freed: %d\n", total_memory_freed);
        */
    }

    // A virtual method that gets called whenever a huge-page is allocated
    // WARNING: This ONLY applies to hooks using BKMalloc. For the time being,
    //          HeapPulse only uses BKMalloc, so this is fine.
    void on_huge_page_alloc(uint8_t *huge_page, size_t size) override {
        // stack_debugf("on_huge_page_alloc\n");
        stack_debugf("Got huge page allocation of size %d at address %p\n", size, huge_page);
        auto page = HugePage(huge_page, size);
        page.written_to = true;
        page.accessed = true;
        huge_page_liveset.insert(page);
    }
    // A virtual method that gets called whenever a huge-page is released
    // WARNING: This ONLY applies to hooks using BKMalloc. For the time being,
    //          HeapPulse only uses BKMalloc, so this is fine.
    void on_huge_page_free(uint8_t *huge_page, size_t size) override {
        // stack_debugf("on_huge_page_free\n");
        stack_debugf("Got huge page free of size %d at address %p\n", size, huge_page);
        huge_page_liveset.remove(HugePage(huge_page, size));
    }

    void on_access(const Allocation &access, bool is_write) override {
        // stack_debugf("on_access\n");
        stack_debugf("Got access %s of allocation:\n", is_write ? "write" : "read");
        stack_debugf("  size: %d\n", access.size);
        stack_debugf("  addr: %p\n", access.ptr);

        if (is_write) {
            write_accessed_this_interval.insert(access);
        } else {
            read_accessed_this_interval.insert(access);
        }

        huge_page_liveset.map([&](HugePage &page) {
            if (page.contains(access)) {
                stack_debugf("Access belongs to page at %p\n", (void*)page.address);
                page.accessed = true;
                if (is_write) {
                    page.written_to = true;
                } else {
                    page.read_from = true;
                }
            } else {
                stack_debugf("Access does not belong to page\n");
            }
        });

        accessed_this_interval.insert(access);
    }


    void on_write(const Allocation &write) override {
        // stack_debugf("on_write\n");
        stack_debugf("Got write of allocation:\n");
        stack_debugf("  size: %d\n", write.size);
        stack_debugf("  addr: %p\n", write.ptr);

        objects_written_to_this_interval++;
        bytes_written_to_this_interval += write.size;
    }

    void on_read(const Allocation &read) override {
        // stack_debugf("on_read\n");
        stack_debugf("Got read of allocation:\n");
        stack_debugf("  size: %d\n", read.size);
        stack_debugf("  addr: %p\n", read.ptr);

        objects_read_from_this_interval++;
        bytes_read_from_this_interval += read.size;
    }

    void on_alloc(const Allocation &alloc) override {
        stack_debugf("on_alloc\n");
        total_objects_live++;
        total_bytes_live += alloc.size;

        // unaccessed_this_interval.insert(alloc);
        live_this_interval.insert(alloc);
        write_accessed_this_interval.insert(alloc);
        read_accessed_this_interval.insert(alloc);
        accessed_this_interval.insert(alloc);
        
        memory_allocated_since_last_interval += alloc.size;
        total_memory_allocated += alloc.size;

        summary();
    }

    void on_free(const Allocation &alloc) override {
        stack_debugf("on_free\n");
        total_objects_live--;
        total_bytes_live -= alloc.size;

        memory_freed_since_last_interval += alloc.size;
        total_memory_freed += alloc.size;

        live_this_interval.remove(alloc);
     
        summary();
    }

    void finish_interval() {
        stack_infof("Live bytes: %d\n", total_bytes_live);
        stack_infof("Live objects: %d\n", total_objects_live);
        stack_infof("Memory allocated since last interval: %d\n", memory_allocated_since_last_interval);
        stack_infof("Memory freed since last interval: %d\n", memory_freed_since_last_interval);
        stack_infof("Total memory allocated: %d\n", total_memory_allocated);
        stack_infof("Total memory freed: %d\n", total_memory_freed);
        summary();

        memory_allocated_since_last_interval = 0;
        memory_freed_since_last_interval = 0;

        objects_written_to_this_interval = 0;
        objects_read_from_this_interval = 0;

        bytes_written_to_this_interval = 0;
        bytes_read_from_this_interval = 0;

        accessed_this_interval.clear();
        write_accessed_this_interval.clear();
        read_accessed_this_interval.clear();

        stack_debugf("Wrote %d rows to file\n", csv.size());
        csv.write(file);
        csv.clear();
        stack_infof("Interval %d complete for access pattern test\n", interval_count);
    }

    void interval(
        const StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> &allocation_sites
    ) override {
        stack_infof("Interval %d access compression test with %s starting...\n", ++interval_count, compression_to_string(compression_type));

        static Compressor<> compressor;
        stack_infof("Compressing %d pages\n", huge_page_liveset.size());
        size_t i=0;
        huge_page_liveset.map([&](HugePage &page) {
            // Align page address
            const size_t HUGE_PAGE_SIZE = 2 * 1024 * 1024;
            auto aligned_address = (uint8_t*)((uintptr_t)page.address & ~(HUGE_PAGE_SIZE - 1));

            // Align the huge page size down to the nearest page size
            if (page.size > HUGE_PAGE_SIZE) {
                page.size = HUGE_PAGE_SIZE;
            }
            if (page.size < HUGE_PAGE_SIZE) {
                return;
            }
            // if (mprotect(aligned_address, HUGE_PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
            //     stack_errorf("Failed to mprotect page %p\n", (void*)aligned_address);
            //     return;
            // }

            // mprotect(page.address, page.size, PROT_READ | PROT_EXEC);
            stack_infof("Compressing page %d/%d\n", ++i, huge_page_liveset.size());
            auto &row = csv.new_row();
            row.set(csv.title(), "Interval #", interval_count);
            row.set(csv.title(), "Huge Page Address", (void*)page.address);
            row.set(csv.title(), "Age (intervals)", page.age);
            row.set(csv.title(), "New?", page.age == 0);
            row.set(csv.title(), "Written?", page.written_to);
            row.set(csv.title(), "Read?", page.read_from);
            row.set(csv.title(), "Unaccessed?", !page.accessed);
            row.set(csv.title(), "Live Pages", huge_page_liveset.size());

            uint64_t compressed_size = compressor.compress((const uint8_t*)page.address, page.size);
            uint64_t uncompressed_size = page.size;

            row.set(csv.title(), "Page Size (bytes)", page.size);
            row.set(csv.title(), "Compressed Size (bytes)", compressed_size);
            if (uncompressed_size == 0) {
                row.set(csv.title(), "Compression Ratio (compressed/uncompressed)", 1.0);
            } else {
                row.set(csv.title(), "Compression Ratio (compressed/uncompressed)", (double)compressed_size / (double)uncompressed_size);
            }
        });

        huge_page_liveset.map([&](HugePage &page) {
            page.tick_age();
            page.clear();
        });

        // Iterate over the allocations and record compression stats + access patterns
        // allocation_sites.map([&](auto return_address, AllocationSite site) {
        //     site.allocations.map([&](void *ptr, Allocation allocation) {
        //         allocation.protect(PROT_READ);
        //         auto &row = csv.new_row();
        //         row.set(csv.title(), "Interval #", interval_count);
        //         row.set(csv.title(), "Object Address", (void*)ptr);
        //         row.set(csv.title(), "Allocation Site", (void*)return_address);
        //         row.set(csv.title(), "Age (intervals)", allocation.age);
        //         row.set(csv.title(), "New?", allocation.get_age() == 0);
        //         row.set(csv.title(), "Written?", write_accessed_this_interval.has(allocation));
        //         row.set(csv.title(), "Read?", read_accessed_this_interval.has(allocation));
        //         row.set(csv.title(), "Unaccessed?", !accessed_this_interval.has(allocation));
        //         row.set(csv.title(), "Live Virtual Bytes", total_bytes_live);

        //         auto physical_pages = allocation.physical_pages<10000>();
        //         uint64_t total_uncompressed_size = 0;
        //         uint64_t total_compressed_size = 0;
        //         physical_pages.map([&](auto page) {
        //             uint64_t uncompressed_size = page.size();
        //             uint64_t compressed_size = 0;
        //             // compress(page.get_virtual_address(), uncompressed_size, compressed_size, compression_type);
        //             compressed_size = compressor.compress((const uint8_t*)page.get_virtual_address(), uncompressed_size);
        //             total_uncompressed_size += uncompressed_size;
        //             total_compressed_size += compressed_size;
        //         });

        //         row.set(csv.title(), "Physical Size (bytes)", total_uncompressed_size);
        //         row.set(csv.title(), "Physical Compressed Size (bytes)", total_compressed_size);
        //         if (total_uncompressed_size == 0) {
        //             row.set(csv.title(), "Physical Compression Ratio (compressed/uncompressed)", 1.0);
        //         } else {
        //             row.set(csv.title(), "Physical Compression Ratio (compressed/uncompressed)", (double)total_compressed_size / (double)total_uncompressed_size);
        //         }
        //     });
        // });

        stack_debugf("Finishing up interval\n");
        finish_interval();

        // Go through and protect allocations
        allocation_sites.map([&](auto return_address, AllocationSite site) {
            site.allocations.map([&](void *ptr, Allocation allocation) {
                allocation.protect();
            });
        });
    }
};
