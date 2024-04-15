#pragma once

#include <interval_test.hpp>
#include <stack_csv.hpp>
#include <zlib.h>

#define min(a, b) ((a) < (b) ? (a) : (b))

// Path: src/compression_test.cpp
class AccessPatternTest : public IntervalTest {
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
                                accessed_last_2_intervals,
                                accessed_last_3_intervals,
                                accessed_last_4_intervals,
                                accessed_last_5_intervals,
                                accessed_last_6_intervals,

                                write_accessed_this_interval,
                                write_accessed_last_2_intervals,
                                write_accessed_last_3_intervals,
                                write_accessed_last_4_intervals,
                                write_accessed_last_5_intervals,
                                write_accessed_last_6_intervals,
                                
                                read_accessed_this_interval,
                                read_accessed_last_2_intervals,
                                read_accessed_last_3_intervals,
                                read_accessed_last_4_intervals,
                                read_accessed_last_5_intervals,
                                read_accessed_last_6_intervals,
                                
                                unaccessed_this_interval,
                                unaccessed_last_2_intervals,
                                unaccessed_last_3_intervals,
                                unaccessed_last_4_intervals,
                                unaccessed_last_5_intervals,
                                unaccessed_last_6_intervals,
                                
                                live_this_interval,
                                live_last_2_intervals,
                                live_last_3_intervals,
                                live_last_4_intervals,
                                live_last_5_intervals,
                                live_last_6_intervals;

    const char *name() const override {
        return "Access Pattern Test";
    }

    void setup() override {
        stack_debugf("Setup\n");
        test_start_time = std::chrono::steady_clock::now();
        test_start_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(test_start_time.time_since_epoch()).count();
        file = StackFile(StackString<256>("access_patterns.csv"), Mode::APPEND);
        file.clear();

        csv.title().add("Interval #");
        csv.title().add("Live Objects");
        csv.title().add("Live Bytes");
        csv.title().add("Memory Allocated Since Last Interval");
        csv.title().add("Memory Freed Since Last Interval");
        csv.title().add("Total Memory Allocated");
        csv.title().add("Total Memory Freed");
        // csv.title().add("Objects Accessed");
        // csv.title().add("Objects Written To");
        // csv.title().add("Objects Read From");
        // csv.title().add("Objects Unaccessed");
        // csv.title().add("Bytes Accessed");
        // csv.title().add("Bytes Written To");
        // csv.title().add("Bytes Read From");
        // csv.title().add("Bytes Unaccessed");

        // Now do interval based access pattern tracking
        csv.title().add("Objects Accessed This Interval");
        csv.title().add("Objects Accessed Last Two Intervals");
        csv.title().add("Objects Accessed Last Three Intervals");
        csv.title().add("Objects Accessed Last Four Intervals");
        csv.title().add("Objects Accessed Last Five Intervals");
        csv.title().add("Objects Accessed Last Six Intervals");

        csv.title().add("Objects Written To This Interval");
        csv.title().add("Objects Written To Last Two Intervals");
        csv.title().add("Objects Written To Last Three Intervals");
        csv.title().add("Objects Written To Last Four Intervals");
        csv.title().add("Objects Written To Last Five Intervals");
        csv.title().add("Objects Written To Last Six Intervals");

        csv.title().add("Objects Read From This Interval");
        csv.title().add("Objects Read From Last Two Intervals");
        csv.title().add("Objects Read From Last Three Intervals");
        csv.title().add("Objects Read From Last Four Intervals");
        csv.title().add("Objects Read From Last Five Intervals");
        csv.title().add("Objects Read From Last Six Intervals");

        csv.title().add("Objects Unaccessed This Interval");
        csv.title().add("Objects Unaccessed Last Two Intervals");
        csv.title().add("Objects Unaccessed Last Three Intervals");
        csv.title().add("Objects Unaccessed Last Four Intervals");
        csv.title().add("Objects Unaccessed Last Five Intervals");
        csv.title().add("Objects Unaccessed Last Six Intervals");

        csv.title().add("Bytes Accessed This Interval");
        csv.title().add("Bytes Accessed Last Two Intervals");
        csv.title().add("Bytes Accessed Last Three Intervals");
        csv.title().add("Bytes Accessed Last Four Intervals");
        csv.title().add("Bytes Accessed Last Five Intervals");
        csv.title().add("Bytes Accessed Last Six Intervals");

        csv.title().add("Bytes Written To This Interval");
        csv.title().add("Bytes Written To Last Two Intervals");
        csv.title().add("Bytes Written To Last Three Intervals");
        csv.title().add("Bytes Written To Last Four Intervals");
        csv.title().add("Bytes Written To Last Five Intervals");
        csv.title().add("Bytes Written To Last Six Intervals");

        csv.title().add("Bytes Read From This Interval");
        csv.title().add("Bytes Read From Last Two Intervals");
        csv.title().add("Bytes Read From Last Three Intervals");
        csv.title().add("Bytes Read From Last Four Intervals");
        csv.title().add("Bytes Read From Last Five Intervals");
        csv.title().add("Bytes Read From Last Six Intervals");

        csv.title().add("Bytes Unaccessed This Interval");
        csv.title().add("Bytes Unaccessed Last Two Intervals");
        csv.title().add("Bytes Unaccessed Last Three Intervals");
        csv.title().add("Bytes Unaccessed Last Four Intervals");
        csv.title().add("Bytes Unaccessed Last Five Intervals");
        csv.title().add("Bytes Unaccessed Last Six Intervals");

        csv.write(file);
        csv.clear();

        interval_count = 0;
    }

    void summary() {
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
    }

    void on_access(const Allocation &access, bool is_write) override {
        // stack_debugf("on_access\n");
        stack_debugf("Got access of allocation:\n");
        stack_debugf("  size: %d\n", access.size);
        stack_debugf("  addr: %p\n", access.ptr);

        // accesses.insert(access);
        unaccessed_this_interval.remove(access);
        unaccessed_last_2_intervals.remove(access);
        unaccessed_last_3_intervals.remove(access);
        unaccessed_last_4_intervals.remove(access);
        unaccessed_last_5_intervals.remove(access);
        unaccessed_last_6_intervals.remove(access);

        if (is_write) {
            write_accessed_this_interval.insert(access);
            write_accessed_last_2_intervals.insert(access);
            write_accessed_last_3_intervals.insert(access);
            write_accessed_last_4_intervals.insert(access);
            write_accessed_last_5_intervals.insert(access);
            write_accessed_last_6_intervals.insert(access);
        } else {
            read_accessed_this_interval.insert(access);
            read_accessed_last_2_intervals.insert(access);
            read_accessed_last_3_intervals.insert(access);
            read_accessed_last_4_intervals.insert(access);
            read_accessed_last_5_intervals.insert(access);
            read_accessed_last_6_intervals.insert(access);
        }

        accessed_this_interval.insert(access);
        accessed_last_2_intervals.insert(access);
        accessed_last_3_intervals.insert(access);
        accessed_last_4_intervals.insert(access);
        accessed_last_5_intervals.insert(access);
        accessed_last_6_intervals.insert(access);
    }

    void tick_accesses() {
        accessed_last_6_intervals.clear();
        accessed_last_6_intervals = accessed_last_5_intervals;
        accessed_last_5_intervals.clear();
        accessed_last_5_intervals = accessed_last_4_intervals;
        accessed_last_4_intervals.clear();
        accessed_last_4_intervals = accessed_last_3_intervals;
        accessed_last_3_intervals.clear();
        accessed_last_3_intervals = accessed_last_2_intervals;
        accessed_last_2_intervals.clear();
        accessed_last_2_intervals = accessed_this_interval;
        accessed_this_interval.clear();

        write_accessed_last_6_intervals.clear();
        write_accessed_last_6_intervals = write_accessed_last_5_intervals;
        write_accessed_last_5_intervals.clear();
        write_accessed_last_5_intervals = write_accessed_last_4_intervals;
        write_accessed_last_4_intervals.clear();
        write_accessed_last_4_intervals = write_accessed_last_3_intervals;
        write_accessed_last_3_intervals.clear();
        write_accessed_last_3_intervals = write_accessed_last_2_intervals;
        write_accessed_last_2_intervals.clear();
        write_accessed_last_2_intervals = write_accessed_this_interval;
        write_accessed_this_interval.clear();

        read_accessed_last_6_intervals.clear();
        read_accessed_last_6_intervals = read_accessed_last_5_intervals;
        read_accessed_last_5_intervals.clear();
        read_accessed_last_5_intervals = read_accessed_last_4_intervals;
        read_accessed_last_4_intervals.clear();
        read_accessed_last_4_intervals = read_accessed_last_3_intervals;
        read_accessed_last_3_intervals.clear();
        read_accessed_last_3_intervals = read_accessed_last_2_intervals;
        read_accessed_last_2_intervals.clear();
        read_accessed_last_2_intervals = read_accessed_this_interval;
        read_accessed_this_interval.clear();

        unaccessed_last_6_intervals.clear();
        unaccessed_last_6_intervals = unaccessed_last_5_intervals;
        unaccessed_last_5_intervals.clear();
        unaccessed_last_5_intervals = unaccessed_last_4_intervals;
        unaccessed_last_4_intervals.clear();
        unaccessed_last_4_intervals = unaccessed_last_3_intervals;
        unaccessed_last_3_intervals.clear();
        unaccessed_last_3_intervals = unaccessed_last_2_intervals;
        unaccessed_last_2_intervals.clear();
        unaccessed_last_2_intervals = unaccessed_this_interval;

        live_last_6_intervals.clear();
        live_last_6_intervals = live_last_5_intervals;
        live_last_5_intervals.clear();
        live_last_5_intervals = live_last_4_intervals;
        live_last_4_intervals.clear();
        live_last_4_intervals = live_last_3_intervals;
        live_last_3_intervals.clear();
        live_last_3_intervals = live_last_2_intervals;
        live_last_2_intervals.clear();
        live_last_2_intervals = live_this_interval;
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

        write_accessed_last_2_intervals.insert(alloc);
        read_accessed_last_2_intervals.insert(alloc);
        accessed_last_2_intervals.insert(alloc);

        write_accessed_last_3_intervals.insert(alloc);
        read_accessed_last_3_intervals.insert(alloc);
        accessed_last_3_intervals.insert(alloc);

        write_accessed_last_4_intervals.insert(alloc);
        read_accessed_last_4_intervals.insert(alloc);
        accessed_last_4_intervals.insert(alloc);

        write_accessed_last_5_intervals.insert(alloc);
        read_accessed_last_5_intervals.insert(alloc);
        accessed_last_5_intervals.insert(alloc);

        write_accessed_last_6_intervals.insert(alloc);
        read_accessed_last_6_intervals.insert(alloc);
        accessed_last_6_intervals.insert(alloc);

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
        live_last_2_intervals.remove(alloc);
        live_last_3_intervals.remove(alloc);
        live_last_4_intervals.remove(alloc);
        live_last_5_intervals.remove(alloc);
        live_last_6_intervals.remove(alloc);

        // unaccessed_this_interval.remove(alloc);
        // unaccessed_last_2_intervals.remove(alloc);
        // unaccessed_last_3_intervals.remove(alloc);
        // unaccessed_last_4_intervals.remove(alloc);
        // unaccessed_last_5_intervals.remove(alloc);
        // unaccessed_last_6_intervals.remove(alloc);

        // Remove from the access pattern tracking
        // accessed_this_interval.remove(alloc);
        // write_accessed_this_interval.remove(alloc);
        // read_accessed_this_interval.remove(alloc);

        // accessed_last_2_intervals.remove(alloc);
        // write_accessed_last_2_intervals.remove(alloc);
        // read_accessed_last_2_intervals.remove(alloc);

        // accessed_last_3_intervals.remove(alloc);
        // write_accessed_last_3_intervals.remove(alloc);
        // read_accessed_last_3_intervals.remove(alloc);

        // accessed_last_4_intervals.remove(alloc);
        // write_accessed_last_4_intervals.remove(alloc);
        // read_accessed_last_4_intervals.remove(alloc);

        // accessed_last_5_intervals.remove(alloc);
        // write_accessed_last_5_intervals.remove(alloc);
        // read_accessed_last_5_intervals.remove(alloc);

        // accessed_last_6_intervals.remove(alloc);
        // write_accessed_last_6_intervals.remove(alloc);
        // read_accessed_last_6_intervals.remove(alloc);
     
        summary();
    }

    void finish_interval() {
        // stack_infof("Total allocations: %d\n", total_allocations);
        // stack_infof("Total frees: %d\n", total_frees);
        // stack_infof("Memory allocated since last interval: %d\n", memory_allocated_since_last_interval);
        // stack_infof("Memory freed since last interval: %d\n", memory_freed_since_last_interval);
        // stack_infof("Total memory allocated: %d\n", total_memory_allocated);
        // stack_infof("Total memory freed: %d\n", total_memory_freed);
        summary();

        memory_allocated_since_last_interval = 0;
        memory_freed_since_last_interval = 0;

        objects_written_to_this_interval = 0;
        objects_read_from_this_interval = 0;

        bytes_written_to_this_interval = 0;
        bytes_read_from_this_interval = 0;

        stack_debugf("Wrote %d rows to file\n", csv.size());
        tick_accesses();
        csv.write(file);
        csv.clear();
        stack_infof("Interval %d complete for access pattern test\n", interval_count);
    }

    void track_accesses(const StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> &allocation_sites) {
        // Go through physical pages and call .access, .on_read, .on_write
        // for every allocation that has a physical page that's been accessed
        allocation_sites.map([&](auto return_address, AllocationSite site) {
            site.allocations.map([&](void *ptr, Allocation allocation) {
                auto pages = allocation.physical_pages<10000>();
                bool tracked = false;
                pages.map([&](auto page) {
                    if (tracked) return;
                    if (page.has_been_read() || page.has_been_written()) {
                        on_access(allocation, page.has_been_written());
                    }

                    if (page.has_been_written()) {
                        on_write(allocation);
                    } else if (page.has_been_read()) {
                        on_read(allocation);
                    }

                    tracked = true;
                });
            });
        });
    }

    void interval(
        const StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> &allocation_sites
    ) override {
        stack_infof("Interval %d generational test starting...\n", ++interval_count);

        #ifndef GUARD_ACCESSES
        track_accesses(allocation_sites);
        #endif

        int64_t objects_accessed_this_interval = accessed_this_interval.size();
        int64_t bytes_accessed_this_interval = accessed_this_interval.reduce<int64_t>([](const Allocation &alloc, int64_t acc) {
            return acc + alloc.size;
        }, 0);

        unaccessed_this_interval = live_this_interval;
        accessed_this_interval.map([&](const Allocation &alloc) {
            unaccessed_this_interval.remove(alloc);
        });

        unaccessed_last_2_intervals = live_last_2_intervals;
        accessed_last_2_intervals.map([&](const Allocation &alloc) {
            unaccessed_last_2_intervals.remove(alloc);
        });

        unaccessed_last_3_intervals = live_last_3_intervals;
        accessed_last_3_intervals.map([&](const Allocation &alloc) {
            unaccessed_last_3_intervals.remove(alloc);
        });

        unaccessed_last_4_intervals = live_last_4_intervals;
        accessed_last_4_intervals.map([&](const Allocation &alloc) {
            unaccessed_last_4_intervals.remove(alloc);
        });

        unaccessed_last_5_intervals = live_last_5_intervals;
        accessed_last_5_intervals.map([&](const Allocation &alloc) {
            unaccessed_last_5_intervals.remove(alloc);
        });

        unaccessed_last_6_intervals = live_last_6_intervals;
        accessed_last_6_intervals.map([&](const Allocation &alloc) {
            unaccessed_last_6_intervals.remove(alloc);
        });

        // int64_t objects_unaccessed_this_interval = total_objects_live - objects_accessed_this_interval;
        // int64_t bytes_unaccessed_this_interval = total_bytes_live - bytes_accessed_this_interval;
        
        CSVRow<64> &row = csv.new_row();
        row.set(csv.title(), "Interval #", interval_count);
        row.set(csv.title(), "Total Memory Allocated", total_memory_allocated);
        row.set(csv.title(), "Total Memory Freed", total_memory_freed);
        row.set(csv.title(), "Memory Allocated Since Last Interval", memory_allocated_since_last_interval);
        row.set(csv.title(), "Memory Freed Since Last Interval", memory_freed_since_last_interval);
        // row.set(csv.title(), "Live Objects", total_objects_live);
        // row.set(csv.title(), "Live Bytes", total_bytes_live);
        int64_t live_bytes = live_this_interval.reduce<int64_t>([](const Allocation &alloc, int64_t acc) {
            return acc + alloc.size;
        }, 0);
        row.set(csv.title(), "Live Objects", live_this_interval.size());
        row.set(csv.title(), "Live Bytes", live_bytes);
        
        row.set(csv.title(), "Objects Accessed This Interval", accessed_this_interval.size());
        row.set(csv.title(), "Objects Written To This Interval", write_accessed_this_interval.size());
        row.set(csv.title(), "Objects Read From This Interval", read_accessed_this_interval.size());
        row.set(csv.title(), "Objects Unaccessed This Interval", unaccessed_this_interval.size());

        row.set(csv.title(), "Bytes Accessed This Interval", bytes_accessed_this_interval);
        row.set(csv.title(), "Bytes Written To This Interval", bytes_written_to_this_interval);
        row.set(csv.title(), "Bytes Read From This Interval", bytes_read_from_this_interval);
        int64_t bytes_unaccessed_this_interval = unaccessed_this_interval.reduce<int64_t>([](const Allocation &alloc, int64_t acc) {
            return acc + alloc.size;
        }, 0);
        row.set(csv.title(), "Bytes Unaccessed This Interval", bytes_unaccessed_this_interval);
        
        row.set(csv.title(), "Objects Accessed Last Two Intervals", accessed_last_2_intervals.size());
        row.set(csv.title(), "Objects Written To Last Two Intervals", write_accessed_last_2_intervals.size());
        row.set(csv.title(), "Objects Read From Last Two Intervals", read_accessed_last_2_intervals.size());
        row.set(csv.title(), "Objects Unaccessed Last Two Intervals", unaccessed_last_2_intervals.size());
        // row.set(csv.title(), "Objects Unaccessed Last Two Intervals", total_objects_live - accessed_last_2_intervals.size());

        int64_t bytes_accessed_last_2_intervals = accessed_last_2_intervals.reduce<int64_t>([](const Allocation &alloc, int64_t acc) {
            return acc + alloc.size;
        }, 0);
        row.set(csv.title(), "Bytes Accessed Last Two Intervals", bytes_accessed_last_2_intervals);
        int64_t bytes_written_to_last_2_intervals = write_accessed_last_2_intervals.reduce<int64_t>([](const Allocation &alloc, int64_t acc) {
            return acc + alloc.size;
        }, 0);
        row.set(csv.title(), "Bytes Written To Last Two Intervals", bytes_written_to_last_2_intervals);
        int64_t bytes_read_from_last_2_intervals = read_accessed_last_2_intervals.reduce<int64_t>([](const Allocation &alloc, int64_t acc) {
            return acc + alloc.size;
        }, 0);
        row.set(csv.title(), "Bytes Read From Last Two Intervals", bytes_read_from_last_2_intervals);
        int64_t bytes_unaccessed_last_2_intervals = unaccessed_last_2_intervals.reduce<int64_t>([](const Allocation &alloc, int64_t acc) {
            return acc + alloc.size;
        }, 0);
        row.set(csv.title(), "Bytes Unaccessed Last Two Intervals", bytes_unaccessed_last_2_intervals);

        row.set(csv.title(), "Objects Accessed Last Three Intervals", accessed_last_3_intervals.size());
        row.set(csv.title(), "Objects Written To Last Three Intervals", write_accessed_last_3_intervals.size());
        row.set(csv.title(), "Objects Read From Last Three Intervals", read_accessed_last_3_intervals.size());
        row.set(csv.title(), "Objects Unaccessed Last Three Intervals", unaccessed_last_3_intervals.size());

        int64_t bytes_accessed_last_3_intervals = accessed_last_3_intervals.reduce<int64_t>([](const Allocation &alloc, int64_t acc) {
            return acc + alloc.size;
        }, 0);
        row.set(csv.title(), "Bytes Accessed Last Three Intervals", bytes_accessed_last_3_intervals);
        int64_t bytes_written_to_last_3_intervals = write_accessed_last_3_intervals.reduce<int64_t>([](const Allocation &alloc, int64_t acc) {
            return acc + alloc.size;
        }, 0);
        row.set(csv.title(), "Bytes Written To Last Three Intervals", bytes_written_to_last_3_intervals);
        int64_t bytes_read_from_last_3_intervals = read_accessed_last_3_intervals.reduce<int64_t>([](const Allocation &alloc, int64_t acc) {
            return acc + alloc.size;
        }, 0);
        row.set(csv.title(), "Bytes Read From Last Three Intervals", bytes_read_from_last_3_intervals);
        int64_t bytes_unaccessed_last_3_intervals = unaccessed_last_3_intervals.reduce<int64_t>([](const Allocation &alloc, int64_t acc) {
            return acc + alloc.size;
        }, 0);
        row.set(csv.title(), "Bytes Unaccessed Last Three Intervals", bytes_unaccessed_last_3_intervals);

        row.set(csv.title(), "Objects Accessed Last Four Intervals", accessed_last_4_intervals.size());
        row.set(csv.title(), "Objects Written To Last Four Intervals", write_accessed_last_4_intervals.size());
        row.set(csv.title(), "Objects Read From Last Four Intervals", read_accessed_last_4_intervals.size());
        row.set(csv.title(), "Objects Unaccessed Last Four Intervals", unaccessed_last_4_intervals.size());

        int64_t bytes_accessed_last_4_intervals = accessed_last_4_intervals.reduce<int64_t>([](const Allocation &alloc, int64_t acc) {
            return acc + alloc.size;
        }, 0);
        row.set(csv.title(), "Bytes Accessed Last Four Intervals", bytes_accessed_last_4_intervals);
        int64_t bytes_written_to_last_4_intervals = write_accessed_last_4_intervals.reduce<int64_t>([](const Allocation &alloc, int64_t acc) {
            return acc + alloc.size;
        }, 0);
        row.set(csv.title(), "Bytes Written To Last Four Intervals", bytes_written_to_last_4_intervals);
        int64_t bytes_read_from_last_4_intervals = read_accessed_last_4_intervals.reduce<int64_t>([](const Allocation &alloc, int64_t acc) {
            return acc + alloc.size;
        }, 0);
        row.set(csv.title(), "Bytes Read From Last Four Intervals", bytes_read_from_last_4_intervals);
        int64_t bytes_unaccessed_last_4_intervals = unaccessed_last_4_intervals.reduce<int64_t>([](const Allocation &alloc, int64_t acc) {
            return acc + alloc.size;
        }, 0);
        row.set(csv.title(), "Bytes Unaccessed Last Four Intervals", bytes_unaccessed_last_4_intervals);
        
        row.set(csv.title(), "Objects Accessed Last Five Intervals", accessed_last_5_intervals.size());
        row.set(csv.title(), "Objects Written To Last Five Intervals", write_accessed_last_5_intervals.size());
        row.set(csv.title(), "Objects Read From Last Five Intervals", read_accessed_last_5_intervals.size());
        row.set(csv.title(), "Objects Unaccessed Last Five Intervals", unaccessed_last_5_intervals.size());

        int64_t bytes_accessed_last_5_intervals = accessed_last_5_intervals.reduce<int64_t>([](const Allocation &alloc, int64_t acc) {
            return acc + alloc.size;
        }, 0);
        row.set(csv.title(), "Bytes Accessed Last Five Intervals", bytes_accessed_last_5_intervals);
        int64_t bytes_written_to_last_5_intervals = write_accessed_last_5_intervals.reduce<int64_t>([](const Allocation &alloc, int64_t acc) {
            return acc + alloc.size;
        }, 0);
        row.set(csv.title(), "Bytes Written To Last Five Intervals", bytes_written_to_last_5_intervals);
        int64_t bytes_read_from_last_5_intervals = read_accessed_last_5_intervals.reduce<int64_t>([](const Allocation &alloc, int64_t acc) {
            return acc + alloc.size;
        }, 0);
        row.set(csv.title(), "Bytes Read From Last Five Intervals", bytes_read_from_last_5_intervals);
        int64_t bytes_unaccessed_last_5_intervals = unaccessed_last_5_intervals.reduce<int64_t>([](const Allocation &alloc, int64_t acc) {
            return acc + alloc.size;
        }, 0);
        row.set(csv.title(), "Bytes Unaccessed Last Five Intervals", unaccessed_last_5_intervals.size());

        row.set(csv.title(), "Objects Accessed Last Six Intervals", accessed_last_6_intervals.size());
        row.set(csv.title(), "Objects Written To Last Six Intervals", write_accessed_last_6_intervals.size());
        row.set(csv.title(), "Objects Read From Last Six Intervals", read_accessed_last_6_intervals.size());
        row.set(csv.title(), "Objects Unaccessed Last Six Intervals", unaccessed_last_6_intervals.size());

        int64_t bytes_accessed_last_6_intervals = accessed_last_6_intervals.reduce<int64_t>([](const Allocation &alloc, int64_t acc) {
            return acc + alloc.size;
        }, 0);
        row.set(csv.title(), "Bytes Accessed Last Six Intervals", bytes_accessed_last_6_intervals);
        int64_t bytes_written_to_last_6_intervals = write_accessed_last_6_intervals.reduce<int64_t>([](const Allocation &alloc, int64_t acc) {
            return acc + alloc.size;
        }, 0);
        row.set(csv.title(), "Bytes Written To Last Six Intervals", bytes_written_to_last_6_intervals);
        int64_t bytes_read_from_last_6_intervals = read_accessed_last_6_intervals.reduce<int64_t>([](const Allocation &alloc, int64_t acc) {
            return acc + alloc.size;
        }, 0);
        row.set(csv.title(), "Bytes Read From Last Six Intervals", bytes_read_from_last_6_intervals);
        int64_t bytes_unaccessed_last_6_intervals = unaccessed_last_6_intervals.reduce<int64_t>([](const Allocation &alloc, int64_t acc) {
            return acc + alloc.size;
        }, 0);
        row.set(csv.title(), "Bytes Unaccessed Last Six Intervals", bytes_unaccessed_last_6_intervals);

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
