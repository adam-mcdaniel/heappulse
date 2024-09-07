#pragma once

#include <interval_test.hpp>
#include <stack_csv.hpp>
#include <zlib.h>

#define min(a, b) ((a) < (b) ? (a) : (b))

// Path: src/compression_test.cpp
class GenerationalTest : public IntervalTest {
    CSV<48, 80000> csv;
    StackFile file;
    size_t interval_count = 0;
    std::chrono::steady_clock::time_point test_start_time;
    int64_t test_start_time_ms;
    
    int64_t total_allocations = 0;
    int64_t total_frees = 0;

    int64_t memory_allocated_since_last_interval = 0;
    int64_t memory_freed_since_last_interval = 0;

    int64_t total_memory_allocated = 0;
    int64_t total_memory_freed = 0;

    const char *name() const override {
        return "Generational Test";
    }

    void setup() override {
        stack_debugf("Setup\n");
        test_start_time = std::chrono::steady_clock::now();
        test_start_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(test_start_time.time_since_epoch()).count();
        file = StackFile(StackString<256>("generational.csv"), Mode::APPEND);
        file.clear();

        csv.title().add("Interval #");
        csv.title().add("Total Memory Allocated");
        csv.title().add("Total Memory Freed");
        csv.title().add("Memory Allocated Since Last Interval");
        csv.title().add("Memory Freed Since Last Interval");

        csv.title().add("Objects Live This Interval Physical Size");
        csv.title().add("Objects Live >=2 Intervals Physical Size");
        csv.title().add("Objects Live >=4 Intervals Physical Size");
        csv.title().add("Objects Live >=8 Intervals Physical Size");
        csv.title().add("Objects Live >=10 Intervals Physical Size");
        csv.title().add("Objects Live >=16 Intervals Physical Size");
        csv.title().add("Objects Live >=24 Intervals Physical Size");
        csv.title().add("Objects Live >=32 Intervals Physical Size");
        
        csv.title().add("Objects Live This Interval Physical Size (Written)");
        csv.title().add("Objects Live >=2 Intervals Physical Size (Written)");
        csv.title().add("Objects Live >=4 Intervals Physical Size (Written)");
        csv.title().add("Objects Live >=8 Intervals Physical Size (Written)");
        csv.title().add("Objects Live >=10 Intervals Physical Size (Written)");
        csv.title().add("Objects Live >=16 Intervals Physical Size (Written)");
        csv.title().add("Objects Live >=24 Intervals Physical Size (Written)");
        csv.title().add("Objects Live >=32 Intervals Physical Size (Written)");

        csv.title().add("Objects Live This Interval Physical Size (Read-Only)");
        csv.title().add("Objects Live >=2 Intervals Physical Size (Read-Only)");
        csv.title().add("Objects Live >=4 Intervals Physical Size (Read-Only)");
        csv.title().add("Objects Live >=8 Intervals Physical Size (Read-Only)");
        csv.title().add("Objects Live >=10 Intervals Physical Size (Read-Only)");
        csv.title().add("Objects Live >=16 Intervals Physical Size (Read-Only)");
        csv.title().add("Objects Live >=24 Intervals Physical Size (Read-Only)");
        csv.title().add("Objects Live >=32 Intervals Physical Size (Read-Only)");

        csv.title().add("Objects Live This Interval Virtual Size");
        csv.title().add("Objects Live >=2 Intervals Virtual Size");
        csv.title().add("Objects Live >=4 Intervals Virtual Size");
        csv.title().add("Objects Live >=8 Intervals Virtual Size");
        csv.title().add("Objects Live >=10 Intervals Virtual Size");
        csv.title().add("Objects Live >=16 Intervals Virtual Size");
        csv.title().add("Objects Live >=24 Intervals Virtual Size");
        csv.title().add("Objects Live >=32 Intervals Virtual Size");

        csv.write(file);
        csv.clear();

        interval_count = 0;
    }

    void summary() {
        stack_infof("Total allocations: %d\n", total_allocations);
        stack_infof("Total frees: %d\n", total_frees);
        stack_infof("Memory allocated since last interval: %d\n", memory_allocated_since_last_interval);
        stack_infof("Memory freed since last interval: %d\n", memory_freed_since_last_interval);
        stack_infof("Total memory allocated: %d\n", total_memory_allocated);
        stack_infof("Total memory freed: %d\n", total_memory_freed);
    }

    void on_alloc(const Allocation &alloc) override {
        stack_debugf("on_alloc\n");
        total_allocations++;

        memory_allocated_since_last_interval += alloc.size;
        total_memory_allocated += alloc.size;

        summary();
    }

    void on_free(const Allocation &alloc) override {
        stack_debugf("on_free\n");
        total_frees++;

        memory_freed_since_last_interval += alloc.size;
        total_memory_freed += alloc.size;
     
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
        stack_debugf("Wrote %d rows to file\n", csv.size());
        csv.write(file);
        csv.clear();
        stack_infof("Interval %d complete for generational test\n", interval_count);
    }

    void interval(
        const StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> &allocation_sites
    ) override {
        stack_infof("Interval %d generational test starting...\n", ++interval_count);

        int64_t objects_live_this_interval_physical_size = 0;
        int64_t objects_live_2_intervals_physical_size = 0;
        int64_t objects_live_4_intervals_physical_size = 0;
        int64_t objects_live_8_intervals_physical_size = 0;
        int64_t objects_live_10_intervals_physical_size = 0;
        int64_t objects_live_16_intervals_physical_size = 0;
        int64_t objects_live_24_intervals_physical_size = 0;
        int64_t objects_live_32_intervals_physical_size = 0;

        int64_t objects_live_this_interval_physical_size_written = 0;
        int64_t objects_live_2_intervals_physical_size_written = 0;
        int64_t objects_live_4_intervals_physical_size_written = 0;
        int64_t objects_live_8_intervals_physical_size_written = 0;
        int64_t objects_live_10_intervals_physical_size_written = 0;
        int64_t objects_live_16_intervals_physical_size_written = 0;
        int64_t objects_live_24_intervals_physical_size_written = 0;
        int64_t objects_live_32_intervals_physical_size_written = 0;

        int64_t objects_live_this_interval_physical_size_readonly = 0;
        int64_t objects_live_2_intervals_physical_size_readonly = 0;
        int64_t objects_live_4_intervals_physical_size_readonly = 0;
        int64_t objects_live_8_intervals_physical_size_readonly = 0;
        int64_t objects_live_10_intervals_physical_size_readonly = 0;
        int64_t objects_live_16_intervals_physical_size_readonly = 0;
        int64_t objects_live_24_intervals_physical_size_readonly = 0;
        int64_t objects_live_32_intervals_physical_size_readonly = 0;

        int64_t objects_live_this_interval_virtual_size = total_memory_allocated - total_memory_freed;
        int64_t objects_live_2_intervals_virtual_size = 0;
        int64_t objects_live_4_intervals_virtual_size = 0;
        int64_t objects_live_8_intervals_virtual_size = 0;
        int64_t objects_live_10_intervals_virtual_size = 0;
        int64_t objects_live_16_intervals_virtual_size = 0;
        int64_t objects_live_24_intervals_virtual_size = 0;
        int64_t objects_live_32_intervals_virtual_size = 0;


        allocation_sites.map([&](auto return_address, AllocationSite site) {
            site.allocations.map([&](void *ptr, Allocation allocation) {
                auto pages = allocation.physical_pages<10000>();
                if (allocation.get_age() >= 2) {
                    objects_live_2_intervals_virtual_size += allocation.size;
                }
                if (allocation.get_age() >= 4) {
                    objects_live_4_intervals_virtual_size += allocation.size;
                }
                if (allocation.get_age() >= 8) {
                    objects_live_8_intervals_virtual_size += allocation.size;
                }
                if (allocation.get_age() >= 10) {
                    objects_live_10_intervals_virtual_size += allocation.size;
                }
                if (allocation.get_age() >= 16) {
                    objects_live_16_intervals_virtual_size += allocation.size;
                }
                if (allocation.get_age() >= 24) {
                    objects_live_24_intervals_virtual_size += allocation.size;
                }
                if (allocation.get_age() >= 32) {
                    objects_live_32_intervals_virtual_size += allocation.size;
                }

                pages.map([&](auto page) {
                    if (page.is_zero()) {
                        return;
                    }
                    objects_live_this_interval_physical_size += min(page.size(), allocation.size);
                    if (page.is_dirty()) {
                        objects_live_this_interval_physical_size_written += min(page.size(), allocation.size);
                    } else {
                        objects_live_this_interval_physical_size_readonly += min(page.size(), allocation.size);
                    }
                    if (allocation.get_age() >= 2) {
                        objects_live_2_intervals_physical_size += min(page.size(), allocation.size);
                        if (page.is_dirty()) {
                            objects_live_2_intervals_physical_size_written += min(page.size(), allocation.size);
                        } else {
                            objects_live_2_intervals_physical_size_readonly += min(page.size(), allocation.size);
                        }
                    }
                    if (allocation.get_age() >= 4) {
                        objects_live_4_intervals_physical_size += min(page.size(), allocation.size);
                        if (page.is_dirty()) {
                            objects_live_4_intervals_physical_size_written += min(page.size(), allocation.size);
                        } else {
                            objects_live_4_intervals_physical_size_readonly += min(page.size(), allocation.size);
                        }
                    }
                    if (allocation.get_age() >= 8) {
                        objects_live_8_intervals_physical_size += min(page.size(), allocation.size);
                        if (page.is_dirty()) {
                            objects_live_8_intervals_physical_size_written += min(page.size(), allocation.size);
                        } else {
                            objects_live_8_intervals_physical_size_readonly += min(page.size(), allocation.size);
                        }
                    }
                    if (allocation.get_age() >= 10) {
                        objects_live_10_intervals_physical_size += min(page.size(), allocation.size);
                        if (page.is_dirty()) {
                            objects_live_10_intervals_physical_size_written += min(page.size(), allocation.size);
                        } else {
                            objects_live_10_intervals_physical_size_readonly += min(page.size(), allocation.size);
                        }
                    }
                    if (allocation.get_age() >= 16) {
                        objects_live_16_intervals_physical_size += min(page.size(), allocation.size);
                        if (page.is_dirty()) {
                            objects_live_16_intervals_physical_size_written += min(page.size(), allocation.size);
                        } else {
                            objects_live_16_intervals_physical_size_readonly += min(page.size(), allocation.size);
                        }
                    }
                    if (allocation.get_age() >= 24) {
                        objects_live_24_intervals_physical_size += min(page.size(), allocation.size);
                        if (page.is_dirty()) {
                            objects_live_24_intervals_physical_size_written += min(page.size(), allocation.size);
                        } else {
                            objects_live_24_intervals_physical_size_readonly += min(page.size(), allocation.size);
                        }
                    }
                    if (allocation.get_age() >= 32) {
                        objects_live_32_intervals_physical_size += min(page.size(), allocation.size);
                        if (page.is_dirty()) {
                            objects_live_32_intervals_physical_size_written += min(page.size(), allocation.size);
                        } else {
                            objects_live_32_intervals_physical_size_readonly += min(page.size(), allocation.size);
                        }
                    }
                });
            });
        });

        
        // csv.new_row();
        // csv.last()[0] = interval_count;
        // csv.last()[1] = total_memory_allocated;
        // csv.last()[2] = total_memory_freed;
        // csv.last()[3] = memory_allocated_since_last_interval;
        // csv.last()[4] = memory_freed_since_last_interval;

        // csv.last()[5] = objects_live_this_interval_physical_size;
        // csv.last()[6] = objects_live_2_intervals_physical_size;
        // csv.last()[7] = objects_live_4_intervals_physical_size;
        // csv.last()[8] = objects_live_8_intervals_physical_size;
        // csv.last()[9] = objects_live_10_intervals_physical_size;
        // csv.last()[10] = objects_live_16_intervals_physical_size;
        // csv.last()[11] = objects_live_24_intervals_physical_size;
        // csv.last()[12] = objects_live_32_intervals_physical_size;

        // csv.last()[13] = objects_live_this_interval_virtual_size;
        // csv.last()[14] = objects_live_2_intervals_virtual_size;
        // csv.last()[15] = objects_live_4_intervals_virtual_size;
        // csv.last()[16] = objects_live_8_intervals_virtual_size;
        // csv.last()[17] = objects_live_10_intervals_virtual_size;
        // csv.last()[18] = objects_live_16_intervals_virtual_size;
        // csv.last()[19] = objects_live_24_intervals_virtual_size;
        // csv.last()[20] = objects_live_32_intervals_virtual_size;

        // csv.last().add(interval_count);
        // csv.last().add(total_memory_allocated);
        // csv.last().add(total_memory_freed);
        // csv.last().add(memory_allocated_since_last_interval);
        // csv.last().add(memory_freed_since_last_interval);

        // csv.last().add(objects_live_this_interval_physical_size);
        // csv.last().add(objects_live_2_intervals_physical_size);
        // csv.last().add(objects_live_4_intervals_physical_size);
        // csv.last().add(objects_live_8_intervals_physical_size);
        // csv.last().add(objects_live_10_intervals_physical_size);
        // csv.last().add(objects_live_16_intervals_physical_size);
        // csv.last().add(objects_live_24_intervals_physical_size);
        // csv.last().add(objects_live_32_intervals_physical_size);

        // csv.last().add(objects_live_this_interval_virtual_size);
        // csv.last().add(objects_live_2_intervals_virtual_size);
        // csv.last().add(objects_live_4_intervals_virtual_size);
        // csv.last().add(objects_live_8_intervals_virtual_size);
        // csv.last().add(objects_live_10_intervals_virtual_size);
        // csv.last().add(objects_live_16_intervals_virtual_size);
        // csv.last().add(objects_live_24_intervals_virtual_size);
        // csv.last().add(objects_live_32_intervals_virtual_size);

        CSVRow<48> &row = csv.new_row();
        row.set(csv.title(), "Interval #", interval_count);
        row.set(csv.title(), "Total Memory Allocated", total_memory_allocated);
        row.set(csv.title(), "Total Memory Freed", total_memory_freed);
        row.set(csv.title(), "Memory Allocated Since Last Interval", memory_allocated_since_last_interval);
        row.set(csv.title(), "Memory Freed Since Last Interval", memory_freed_since_last_interval);

        row.set(csv.title(), "Objects Live This Interval Physical Size", objects_live_this_interval_physical_size);
        row.set(csv.title(), "Objects Live >=2 Intervals Physical Size", objects_live_2_intervals_physical_size);
        row.set(csv.title(), "Objects Live >=4 Intervals Physical Size", objects_live_4_intervals_physical_size);
        row.set(csv.title(), "Objects Live >=8 Intervals Physical Size", objects_live_8_intervals_physical_size);
        row.set(csv.title(), "Objects Live >=10 Intervals Physical Size", objects_live_10_intervals_physical_size);
        row.set(csv.title(), "Objects Live >=16 Intervals Physical Size", objects_live_16_intervals_physical_size);
        row.set(csv.title(), "Objects Live >=24 Intervals Physical Size", objects_live_24_intervals_physical_size);
        row.set(csv.title(), "Objects Live >=32 Intervals Physical Size", objects_live_32_intervals_physical_size);

        row.set(csv.title(), "Objects Live This Interval Physical Size (Written)", objects_live_this_interval_physical_size_written);
        row.set(csv.title(), "Objects Live >=2 Intervals Physical Size (Written)", objects_live_2_intervals_physical_size_written);
        row.set(csv.title(), "Objects Live >=4 Intervals Physical Size (Written)", objects_live_4_intervals_physical_size_written);
        row.set(csv.title(), "Objects Live >=8 Intervals Physical Size (Written)", objects_live_8_intervals_physical_size_written);
        row.set(csv.title(), "Objects Live >=10 Intervals Physical Size (Written)", objects_live_10_intervals_physical_size_written);
        row.set(csv.title(), "Objects Live >=16 Intervals Physical Size (Written)", objects_live_16_intervals_physical_size_written);
        row.set(csv.title(), "Objects Live >=24 Intervals Physical Size (Written)", objects_live_24_intervals_physical_size_written);
        row.set(csv.title(), "Objects Live >=32 Intervals Physical Size (Written)", objects_live_32_intervals_physical_size_written);

        row.set(csv.title(), "Objects Live This Interval Physical Size (Read-Only)", objects_live_this_interval_physical_size_readonly);
        row.set(csv.title(), "Objects Live >=2 Intervals Physical Size (Read-Only)", objects_live_2_intervals_physical_size_readonly);
        row.set(csv.title(), "Objects Live >=4 Intervals Physical Size (Read-Only)", objects_live_4_intervals_physical_size_readonly);
        row.set(csv.title(), "Objects Live >=8 Intervals Physical Size (Read-Only)", objects_live_8_intervals_physical_size_readonly);
        row.set(csv.title(), "Objects Live >=10 Intervals Physical Size (Read-Only)", objects_live_10_intervals_physical_size_readonly);
        row.set(csv.title(), "Objects Live >=16 Intervals Physical Size (Read-Only)", objects_live_16_intervals_physical_size_readonly);
        row.set(csv.title(), "Objects Live >=24 Intervals Physical Size (Read-Only)", objects_live_24_intervals_physical_size_readonly);
        row.set(csv.title(), "Objects Live >=32 Intervals Physical Size (Read-Only)", objects_live_32_intervals_physical_size_readonly);

        row.set(csv.title(), "Objects Live This Interval Virtual Size", objects_live_this_interval_virtual_size);
        row.set(csv.title(), "Objects Live >=2 Intervals Virtual Size", objects_live_2_intervals_virtual_size);
        row.set(csv.title(), "Objects Live >=4 Intervals Virtual Size", objects_live_4_intervals_virtual_size);
        row.set(csv.title(), "Objects Live >=8 Intervals Virtual Size", objects_live_8_intervals_virtual_size);
        row.set(csv.title(), "Objects Live >=10 Intervals Virtual Size", objects_live_10_intervals_virtual_size);
        row.set(csv.title(), "Objects Live >=16 Intervals Virtual Size", objects_live_16_intervals_virtual_size);
        row.set(csv.title(), "Objects Live >=24 Intervals Virtual Size", objects_live_24_intervals_virtual_size);
        row.set(csv.title(), "Objects Live >=32 Intervals Virtual Size", objects_live_32_intervals_virtual_size);

        stack_debugf("Finishing up interval\n");
        finish_interval();
    }
};
