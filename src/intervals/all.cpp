#pragma once

#include <config.hpp>
#include <interval_test.hpp>
#include <stack_csv.hpp>
#include <compressor.hpp>

#define min(a, b) ((a) < (b) ? (a) : (b))

// #define TRACK_ACCESSES

#define TRACK_OBJECTS
#define TRACK_PAGES
#define TRACK_HUGE_PAGES

// #define MAX_COMPRESSED_SIZE 0x300000
static uint8_t compressed_buffer[0x20000000];

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

    uint64_t count_overlapping_bytes(void *obj_ptr, uint64_t obj_size) const {
        // Convert all to uintptr_t for pointer arithmetic
        uintptr_t obj_start = (uintptr_t) obj_ptr;
        uintptr_t obj_end = obj_start + obj_size;
        uintptr_t page_start = (uintptr_t) address;
        uintptr_t page_end = (uintptr_t) (address + size);

        // Check if object is completely outside the page
        if (obj_end <= page_start || obj_start >= page_end) {
            return 0;  // No overlap
        }

        // Calculate the overlap between object range and page range
        uintptr_t overlap_start = (obj_start > page_start) ? obj_start : page_start;
        uintptr_t overlap_end = (obj_end < page_end) ? obj_end : page_end;

        // Return the number of overlapping bytes
        return overlap_end - overlap_start;
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

class AllTest : public IntervalTest {
public:
    AllTest() : IntervalTest() {
        init_compression();
    }

private:
    CSV<20, 10000> object_csv, page_csv, huge_page_csv, interval_csv;
    StackFile object_file, page_file, huge_page_file, interval_file;
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

    StackSet<HugePage, 30000> huge_page_liveset;
    StackSet<Allocation, 30000> accessed_this_interval,
                                write_accessed_this_interval,
                                read_accessed_this_interval,
                                live_this_interval;

    const char *name() const override {
        return "All Test";
    }

    void setup() override {
        stack_debugf("Setup\n");
        test_start_time = std::chrono::steady_clock::now();
        test_start_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(test_start_time.time_since_epoch()).count();

        object_file = StackFile(StackString<256>("object-compression.csv"), Mode::APPEND);
        object_file.clear();
        page_file = StackFile(StackString<256>("page-compression.csv"), Mode::APPEND);
        page_file.clear();
        huge_page_file = StackFile(StackString<256>("huge-page-compression.csv"), Mode::APPEND);
        huge_page_file.clear();
        interval_file = StackFile(StackString<256>("interval-info.csv"), Mode::APPEND);
        interval_file.clear();

        object_csv.title().add("Interval #");
        object_csv.title().add("Allocation Site");
        object_csv.title().add("Age (intervals)");
        object_csv.title().add("Age Class");
        object_csv.title().add("Object Address");
        object_csv.title().add("Size (bytes)");
        object_csv.title().add("Compression Type");
        object_csv.title().add("Compressed Size (bytes)");
        object_csv.title().add("Compression Ratio (compressed/uncompressed)");
        object_csv.title().add("Compression Class"); // 0-10%, 10-20%, 20-30%, 30-40%, 40-50%, 50-60%, 60-70%, 70-80%, 80-90%, 90-100%
        object_csv.title().add("Access Type"); // Read, Read/Write

        page_csv.title().add("Interval #");
        page_csv.title().add("Allocation Site");
        page_csv.title().add("Age (intervals)");
        page_csv.title().add("Age Class");
        page_csv.title().add("Virtual Page Address");
        page_csv.title().add("Physical Page Address");
        page_csv.title().add("Size (bytes)");
        page_csv.title().add("Compression Type");
        page_csv.title().add("Compressed Size (bytes)");
        page_csv.title().add("Compression Ratio (compressed/uncompressed)");
        page_csv.title().add("Compression Class"); // 0-10%, 10-20%, 20-30%, 30-40%, 40-50%, 50-60%, 60-70%, 70-80%, 80-90%, 90-100%
        page_csv.title().add("Access Type"); // Read, Read/Write
        page_csv.title().add("Size Occupied (bytes)");

        huge_page_csv.title().add("Interval #");
        huge_page_csv.title().add("Age (intervals)");
        huge_page_csv.title().add("Age Class");
        huge_page_csv.title().add("Huge Page Address");
        huge_page_csv.title().add("Page Address");
        huge_page_csv.title().add("Size (bytes)");
        huge_page_csv.title().add("Compression Type");
        huge_page_csv.title().add("Compressed Size (bytes)");
        huge_page_csv.title().add("Compression Ratio (compressed/uncompressed)");
        huge_page_csv.title().add("Compression Class"); // 0-10%, 10-20%, 20-30%, 30-40%, 40-50%, 50-60%, 60-70%, 70-80%, 80-90%, 90-100%
        huge_page_csv.title().add("Access Type"); // Read, Read/Write
        huge_page_csv.title().add("Size Occupied (bytes)");

        interval_csv.title().add("Interval #");
        interval_csv.title().add("Live Objects");
        interval_csv.title().add("Live Virtual Bytes");
        interval_csv.title().add("Live Huge Pages (2MB)");
        interval_csv.title().add("Resident Physical Pages (4K)");
        interval_csv.title().add("Resident Set Size (bytes)");
        interval_csv.title().add("Total Memory Allocated");
        interval_csv.title().add("Total Memory Freed");
        interval_csv.title().add("Memory Allocated This Interval");
        interval_csv.title().add("Memory Freed This Interval");

        // interval_csv.title().add("Compression Type");
        // interval_csv.title().add("Compressed Size (bytes)");
        // interval_csv.title().add("Compression Ratio (compressed/uncompressed)");
        // interval_csv.title().add("Compression Class");

        #ifdef TRACK_ACCESSES
        object_csv.title().add("Accessed?");
        object_csv.title().add("Read?");
        object_csv.title().add("Written?");
        object_csv.title().add("Unaccessed?");
        page_csv.title().add("Accessed?");
        page_csv.title().add("Read?");
        page_csv.title().add("Written?");
        page_csv.title().add("Unaccessed?");
        huge_page_csv.title().add("Accessed?");
        huge_page_csv.title().add("Read?");
        huge_page_csv.title().add("Written?");
        huge_page_csv.title().add("Unaccessed?");
        #endif

        object_csv.write(object_file);
        object_csv.clear();

        page_csv.write(page_file);
        page_csv.clear();
        
        huge_page_csv.write(huge_page_file);
        huge_page_csv.clear();

        interval_csv.write(interval_file);
        interval_csv.clear();

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

        object_csv.write(object_file);
        object_csv.clear();
        stack_debugf("Wrote %d rows to object file\n", object_csv.size());
        page_csv.write(page_file);
        page_csv.clear();
        stack_debugf("Wrote %d rows to page file\n", page_csv.size());
        huge_page_csv.write(huge_page_file);
        huge_page_csv.clear();
        stack_debugf("Wrote %d rows to huge page file\n", huge_page_csv.size());
        interval_csv.write(interval_file);
        interval_csv.clear();
        stack_debugf("Wrote %d rows to interval file\n", interval_csv.size());
        stack_infof("Interval %d complete for %s test\n", interval_count, name());
    }

    bool is_write(Allocation &alloc) const {
        auto physical_4k_pages = alloc.physical_pages<30000>();
        bool is_write = physical_4k_pages.reduce<bool>([&](auto page, bool acc) {
            return acc || page.is_dirty();
        }, false);
        return is_write;
    }

    bool is_write(const HugePage &page) const {
        Allocation alloc = Allocation(page.address, page.size);
        return is_write(alloc);
    }

    bool is_write(const PageInfo &page_info) const {
        return page_info.is_dirty();
    }

    CSVString age_class(uint64_t age) {
        if (age == 0) {
            return "New";
        } else if (age < 5) {
            return "Young";
        } else if (age < 10) {
            return "Middle-aged";
        } else {
            return "Old";
        }
    }

    CSVString compression_class(size_t compressed_size, size_t uncompressed_size) {
        if (uncompressed_size == 0) {
            return "N/A";
        }

        double ratio = (double)compressed_size / (double)uncompressed_size;
        if (ratio < 0.1) {
            return "0-10%";
        } else if (ratio < 0.2) {
            return "10-20%";
        } else if (ratio < 0.3) {
            return "20-30%";
        } else if (ratio < 0.4) {
            return "30-40%";
        } else if (ratio < 0.5) {
            return "40-50%";
        } else if (ratio < 0.6) {
            return "50-60%";
        } else if (ratio < 0.7) {
            return "60-70%";
        } else if (ratio < 0.8) {
            return "70-80%";
        } else if (ratio < 0.9) {
            return "80-90%";
        } else {
            return "90-100%";
        }
    }

    void track_huge_pages(const StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> &allocation_sites, CompressionType compression_type) {
        Compressor<sizeof(compressed_buffer), false> compressor(compression_type);
        huge_page_liveset.map([&](HugePage &page) {
            auto &row = huge_page_csv.new_row();
            row.set(huge_page_csv.title(), "Interval #", interval_count);
            row.set(huge_page_csv.title(), "Huge Page Address", (void*)page.address);
            row.set(huge_page_csv.title(), "Age (intervals)", page.age);
            row.set(huge_page_csv.title(), "Age Class", age_class(page.age));
            row.set(huge_page_csv.title(), "Size (bytes)", page.size);
            row.set(huge_page_csv.title(), "Compression Type", compression_to_string(compression_type));
            uint64_t uncompressed_size = page.size;
            uint64_t compressed_size = compressor.compress((const uint8_t*)page.address, uncompressed_size, compressed_buffer, sizeof(compressed_buffer));
            row.set(huge_page_csv.title(), "Compressed Size (bytes)", compressed_size);
            if (uncompressed_size == 0) {
                row.set(huge_page_csv.title(), "Compression Ratio (compressed/uncompressed)", 1.0);
            } else {
                row.set(huge_page_csv.title(), "Compression Ratio (compressed/uncompressed)", (double)compressed_size / (double)uncompressed_size);
            }
            // Compression class
            row.set(huge_page_csv.title(), "Compression Class", compression_class(compressed_size, uncompressed_size));
            row.set(huge_page_csv.title(), "Access Type", is_write(page) ? "Read/Write" : "Read");
            row.set(huge_page_csv.title(), "Size Occupied (bytes)", count_bytes_used_huge_page(allocation_sites, page));


            #ifdef TRACK_ACCESSES
            row.set(huge_page_csv.title(), "Accessed?", page.accessed);
            row.set(huge_page_csv.title(), "Read?", page.read_from);
            row.set(huge_page_csv.title(), "Written?", page.written_to);
            row.set(huge_page_csv.title(), "Unaccessed?", !page.accessed);
            #endif

            if (huge_page_csv.full()) {
                huge_page_csv.write(huge_page_file);
                huge_page_csv.clear();
            }
        });
    }

    uint64_t count_bytes_used_4k_page(const StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> &allocation_sites, PageInfo const& page) {
        // Count all the objects that have an address in the page range, and
        // count how many of the bytes exist in the page boundaries
        uint64_t total_bytes_used_in_4k_page = 0;
        allocation_sites.map([&](auto return_address, AllocationSite site) {
            site.allocations.map([&](void *ptr, Allocation allocation) {
                total_bytes_used_in_4k_page += page.count_overlapping_bytes(allocation.ptr, allocation.size);
            });
        });
        return total_bytes_used_in_4k_page;
    }

    uint64_t count_bytes_used_huge_page(const StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> &allocation_sites, HugePage const& page) {
        // Count all the objects that have an address in the page range, and
        // count how many of the bytes exist in the page boundaries
        uint64_t total_bytes_used_huge_page = 0;
        allocation_sites.map([&](auto return_address, AllocationSite site) {
            site.allocations.map([&](void *ptr, Allocation allocation) {
                total_bytes_used_huge_page += page.count_overlapping_bytes(allocation.ptr, allocation.size);
            });
        });
        return total_bytes_used_huge_page;
    }

    void track_physical_pages(const StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> &allocation_sites, CompressionType compression_type) {
        Compressor<sizeof(compressed_buffer), false> compressor(compression_type);

        static StackSet<PageInfo, 10000000> tracked_pages;
        tracked_pages.clear();
        allocation_sites.map([&](auto return_address, AllocationSite site) {
            site.allocations.map([&](void *ptr, Allocation allocation) {
                auto physical_pages = allocation.physical_pages<30000>();

                physical_pages.map([&](auto page_info) {
                    if (tracked_pages.has(page_info)) {
                        return;
                    } else {
                        tracked_pages.insert(page_info);
                    }
                    auto &row = page_csv.new_row();
                    row.set(page_csv.title(), "Interval #", interval_count);
                    row.set(page_csv.title(), "Allocation Site", (void*)return_address);
                    row.set(page_csv.title(), "Age (intervals)", allocation.age);
                    row.set(page_csv.title(), "Age Class", age_class(allocation.age));
                    row.set(page_csv.title(), "Virtual Page Address", (void*)page_info.get_virtual_address());
                    row.set(page_csv.title(), "Physical Page Address", (void*)page_info.get_physical_address());
                    row.set(page_csv.title(), "Size (bytes)", page_info.size());
                    row.set(page_csv.title(), "Compression Type", compression_to_string(compression_type));
                    uint64_t uncompressed_size = page_info.size();
                    uint64_t compressed_size = compressor.compress((const uint8_t*)page_info.get_virtual_address(), uncompressed_size, compressed_buffer, sizeof(compressed_buffer));
                    row.set(page_csv.title(), "Compressed Size (bytes)", compressed_size);
                    if (uncompressed_size == 0) {
                        row.set(page_csv.title(), "Compression Ratio (compressed/uncompressed)", 1.0);
                    } else {
                        row.set(page_csv.title(), "Compression Ratio (compressed/uncompressed)", (double)compressed_size / (double)uncompressed_size);
                    }
                    // Compression class
                    row.set(page_csv.title(), "Compression Class", compression_class(compressed_size, uncompressed_size));
                    row.set(page_csv.title(), "Access Type", is_write(page_info) ? "Read/Write" : "Read");
                    row.set(page_csv.title(), "Size Occupied (bytes)", count_bytes_used_4k_page(allocation_sites, page_info));

                    #ifdef TRACK_ACCESSES
                    //! TODO
                    #endif

                    if (page_csv.full()) {
                        page_csv.write(page_file);
                        page_csv.clear();
                    }
                });
            });
        });
    }

    void track_objects(const StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> &allocation_sites, CompressionType compression_type) {
        Compressor<sizeof(compressed_buffer), false> compressor(compression_type);
        int i = 0;
        allocation_sites.map([&](auto return_address, AllocationSite site) {
            site.allocations.map([&](void *ptr, Allocation allocation) {
                if (i++ > 8000) return;
                // allocation.protect(PROT_READ);
                auto &row = object_csv.new_row();
                row.set(object_csv.title(), "Interval #", interval_count);
                row.set(object_csv.title(), "Object Address", (void*)ptr);
                row.set(object_csv.title(), "Allocation Site", (void*)return_address);
                row.set(object_csv.title(), "Age (intervals)", allocation.age);
                row.set(object_csv.title(), "Age Class", age_class(allocation.age));
                row.set(object_csv.title(), "Size (bytes)", allocation.size);
                row.set(object_csv.title(), "Compression Type", compression_to_string(compression_type));
                uint64_t uncompressed_size = min(allocation.size, sizeof(compressed_buffer));
                uint64_t compressed_size = compressor.compress((const uint8_t*)ptr, uncompressed_size, compressed_buffer, sizeof(compressed_buffer));
                row.set(object_csv.title(), "Compressed Size (bytes)", compressed_size);
                if (uncompressed_size == 0) {
                    row.set(object_csv.title(), "Compression Ratio (compressed/uncompressed)", 1.0);
                } else {
                    row.set(object_csv.title(), "Compression Ratio (compressed/uncompressed)", (double)compressed_size / (double)uncompressed_size);
                }
                // Compression class
                row.set(object_csv.title(), "Compression Class", compression_class(compressed_size, uncompressed_size));
                row.set(object_csv.title(), "Access Type", is_write(allocation) ? "Read/Write" : "Read");

                #ifdef TRACK_ACCESSES
                row.set(object_csv.title(), "Accessed?", accessed_this_interval.has(allocation));
                row.set(object_csv.title(), "Read?", read_accessed_this_interval.has(allocation));
                row.set(object_csv.title(), "Written?", write_accessed_this_interval.has(allocation));
                row.set(object_csv.title(), "Unaccessed?", !accessed_this_interval.has(allocation));
                #endif

                if (object_csv.full()) {
                    object_csv.write(object_file);
                    object_csv.clear();
                }
            });
        });
    }

    void track_interval_info(
        const StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> &allocation_sites
    ) {
        auto &row = interval_csv.new_row();
        row.set(interval_csv.title(), "Interval #", interval_count);
        row.set(interval_csv.title(), "Live Objects", total_objects_live);
        row.set(interval_csv.title(), "Live Virtual Bytes", total_bytes_live);
        row.set(interval_csv.title(), "Live Huge Pages (2MB)", huge_page_liveset.size());

        static StackSet<PageInfo, 10000000> tracked_pages;
        tracked_pages.clear();
        allocation_sites.map([&](auto return_address, AllocationSite site) {
            site.allocations.map([&](void *ptr, Allocation allocation) {
            auto physical_pages = allocation.physical_pages<30000>();

                physical_pages.map([&](auto page_info) {
                    tracked_pages.insert(page_info);
                });
            });
        });

        row.set(interval_csv.title(), "Resident Physical Pages (4K)", tracked_pages.size());
        row.set(interval_csv.title(), "Resident Set Size (bytes)", tracked_pages.size() * 4096);

        row.set(interval_csv.title(), "Total Memory Allocated", total_memory_allocated);
        row.set(interval_csv.title(), "Total Memory Freed", total_memory_freed);
        row.set(interval_csv.title(), "Memory Allocated This Interval", memory_allocated_since_last_interval);
        row.set(interval_csv.title(), "Memory Freed This Interval", memory_freed_since_last_interval);
    }

    void interval(
        const StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> &allocation_sites
    ) override {
        stack_infof("Interval #%d: %s starting...\n", ++interval_count, name());

        // CompressionType types[] = {CompressionType::NONE, CompressionType::LZ4, CompressionType::ZSTD};
        // for (auto type : types) {
        //     track_objects(allocation_sites, type);
        //     track_physical_pages(allocation_sites, type);
        //     track_huge_pages(type);
        // }

        auto types = Compressor<>::supported_compression_types();
        types.map([&](CompressionType type) {
            stack_infof("Tracking with compression type %s\n", compression_to_string(type));
            #ifdef TRACK_OBJECTS
            track_objects(allocation_sites, type);
            #endif
            #ifdef TRACK_PAGES
            track_physical_pages(allocation_sites, type);
            #endif
            #ifdef TRACK_HUGE_PAGES
            track_huge_pages(allocation_sites, type);
            #endif
        });

        track_interval_info(allocation_sites);


        huge_page_liveset.map([&](HugePage &page) {
            page.tick_age();
            page.clear();
        });


        stack_debugf("Finishing up interval\n");
        finish_interval();

        // #ifdef TRACK_ACCESSES
        // // Go through and protect allocations
        // allocation_sites.map([&](auto return_address, AllocationSite site) {
        //     site.allocations.map([&](void *ptr, Allocation allocation) {
        //         allocation.protect();
        //     });
        // });
        // #endif
    }
};
