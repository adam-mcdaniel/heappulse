#pragma once

#include <interval_test.hpp>
#include <stack_csv.hpp>
#include <zlib.h>

#define min(a, b) ((a) < (b) ? (a) : (b))

// Path: src/compression_test.cpp
#define MAX_COMPRESSED_SIZE 0x100000
#define MAX_PAGES 0x10000

static uint8_t buffer[MAX_COMPRESSED_SIZE];

void compress(void *buf, uint64_t &uncompressed_size, uint64_t &compressed_size) {
    stack_debugf("Compressing buffer at %p\n", buf);
    stack_debugf("  size: %d\n", uncompressed_size);

    compressed_size = MAX_COMPRESSED_SIZE;
    if (uncompressed_size > MAX_COMPRESSED_SIZE) {
        stack_warnf("Buffer too large to compress, truncating\n");
        uncompressed_size = MAX_COMPRESSED_SIZE;
    }

    int result = compress2(buffer, (uLongf*)&compressed_size, (const Bytef*)buf, uncompressed_size, Z_BEST_COMPRESSION);
    if (result != Z_OK) {
        stack_errorf("Failed to compress buffer at %p\n", buf);
        stack_errorf("  size: %d\n", uncompressed_size);
        stack_errorf("  compressed size: %d\n", compressed_size);
        stack_errorf("  result: %d\n", result);
        return;
    }

    stack_debugf("Compressed buffer at %p\n", buf);
    stack_debugf("  size: %d\n", uncompressed_size);
    stack_debugf("  compressed size: %d\n", compressed_size);
}

void compress(Allocation &alloc, uint64_t &uncompressed_size, uint64_t &compressed_size) {
    compress(alloc.ptr, uncompressed_size, compressed_size);
}


// Path: src/compression_test.cpp
class AccessCompressionTest : public IntervalTest {
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

    StackSet<Allocation, 100000> accessed_this_interval,
                                write_accessed_this_interval,
                                read_accessed_this_interval,
                                live_this_interval;

    const char *name() const override {
        return "Access Compression Test";
    }

    void setup() override {
        stack_debugf("Setup\n");
        test_start_time = std::chrono::steady_clock::now();
        test_start_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(test_start_time.time_since_epoch()).count();
        file = StackFile(StackString<256>("access-compression.csv"), Mode::APPEND);
        file.clear();


        csv.title().add("Interval #");
        csv.title().add("Object Address");
        csv.title().add("Allocation Site");
        csv.title().add("Age (intervals)");
        csv.title().add("Virtual Size (bytes)");
        csv.title().add("Physical Size (bytes)");
        csv.title().add("Physical Compressed Size (bytes)");
        csv.title().add("New?");
        csv.title().add("Written?");
        csv.title().add("Read?");
        csv.title().add("Unaccessed?");
        csv.title().add("Physical Compression Ratio (compressed/uncompressed)");
        csv.title().add("Live Virtual Bytes");


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
        stack_infof("Interval %d generational test starting...\n", ++interval_count);


        // Iterate over the allocations and record compression stats + access patterns
        allocation_sites.map([&](auto return_address, AllocationSite site) {
            site.allocations.map([&](void *ptr, Allocation allocation) {
                auto &row = csv.new_row();
                row.set(csv.title(), "Interval #", interval_count);
                row.set(csv.title(), "Object Address", (void*)ptr);
                row.set(csv.title(), "Allocation Site", (void*)return_address);
                row.set(csv.title(), "Age (intervals)", allocation.age);
                row.set(csv.title(), "New?", allocation.get_age() == 0);
                row.set(csv.title(), "Written?", write_accessed_this_interval.has(allocation));
                row.set(csv.title(), "Read?", read_accessed_this_interval.has(allocation));
                row.set(csv.title(), "Unaccessed?", !accessed_this_interval.has(allocation));
                row.set(csv.title(), "Live Virtual Bytes", total_bytes_live);

                auto physical_pages = allocation.physical_pages<10000>();
                uint64_t total_uncompressed_size = 0;
                uint64_t total_compressed_size = 0;
                physical_pages.map([&](auto page) {
                    uint64_t uncompressed_size = page.size();
                    uint64_t compressed_size = 0;
                    compress(page.get_virtual_address(), uncompressed_size, compressed_size);
                    total_uncompressed_size += uncompressed_size;
                    total_compressed_size += compressed_size;
                });

                row.set(csv.title(), "Virtual Size (bytes)", allocation.size);
                row.set(csv.title(), "Physical Size (bytes)", total_uncompressed_size);
                row.set(csv.title(), "Physical Compressed Size (bytes)", total_compressed_size);
                if (total_uncompressed_size == 0) {
                    row.set(csv.title(), "Physical Compression Ratio (compressed/uncompressed)", 1.0);
                } else {
                    row.set(csv.title(), "Physical Compression Ratio (compressed/uncompressed)", (double)total_compressed_size / (double)total_uncompressed_size);
                }
            });
        });

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
