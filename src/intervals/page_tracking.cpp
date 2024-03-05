#pragma once

#include <interval_test.hpp>
#include <stack_csv.hpp>
#include <zlib.h>
#include <stack_map.hpp>
#include <stack_vec.hpp>

struct PageTracking {
    bool read_only_after_initial_write = true;
    bool has_new_objects = false;
    bool modified_this_interval = false;
    bool modified_last_interval = false;
    uint64_t first_interval = 0;
    uint64_t age_in_intervals = 0;
    uint64_t write_count = 0;
    uint64_t age_since_last_write = 0;
    uintptr_t virtual_address = 0;
    uintptr_t physical_address = 0;

    PageTracking() {}

    PageTracking(int interval, uintptr_t virtual_address, uintptr_t physical_address)
        : first_interval(interval), virtual_address(virtual_address), physical_address(physical_address) {}

    void write() {
        age_since_last_write = 0;
        write_count++;
        if (age_in_intervals <= 1) {
            read_only_after_initial_write = false;
        }
        modified_this_interval = true;
    }

    void no_write() {
        modified_this_interval = false;
    }

    void age() {
        age_in_intervals++;
        age_since_last_write++;
        modified_last_interval = modified_this_interval;
        modified_this_interval = false;
    }

    void add_new_objects() {
        has_new_objects = true;
    }
};

// Path: src/compression_test.cpp
class PageTrackingTest : public IntervalTest {
    CSV<12, 100000> csv;
    StackFile file;
    size_t interval_count = 0;
    std::chrono::steady_clock::time_point test_start_time;
    uint64_t test_start_time_ms;

    StackMap<void*, PageTracking, 1000000> tracking;

    const char *name() const override {
        return "Page Tracking Test";
    }

    void setup() override {
        stack_debugf("Setup\n");
        test_start_time = std::chrono::steady_clock::now();
        test_start_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(test_start_time.time_since_epoch()).count();
        file = StackFile(StackString<256>("page-tracking.csv"), Mode::APPEND);
        file.clear();

        csv.title().add("Interval #"); // 0
        csv.title().add("Allocation Site"); // 1
        csv.title().add("Age (intervals)"); // 2
        csv.title().add("Is New Page?"); // 3
        csv.title().add("Virtual Page Address"); // 4
        csv.title().add("Physical Page Address"); // 5
        csv.title().add("Modified This Interval?"); // 6
        csv.title().add("Modified Last Interval?"); // 7
        csv.title().add("Has New Objects?"); // 8
        csv.title().add("Write Count"); // 9
        csv.title().add("Age Since Last Write"); // 10

        csv.write(file);
        csv.clear();
        interval_count = 0;
    }

    void interval(
        const StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> &allocation_sites
        // const StackVec<Allocation, TOTAL_TRACKED_ALLOCATIONS> &allocations
    ) override {
        stack_infof("Interval %d page liveness starting...\n", ++interval_count);
        allocation_sites.map([&](auto return_address, AllocationSite site) {
            stack_infof("Site 0x%x\n", return_address);
            site.allocations.map([&](void *ptr, Allocation allocation) {
                auto pages = allocation.physical_pages<20000>();
                pages.map([&](auto page) {
                    if (page.is_zero()) {
                        return;
                    }
                    csv.new_row();
                    csv.last()[0] = interval_count;
                    csv.last()[1] = CSVString::format("0x%x", site.return_address);
                    csv.last()[3] = is_page_new(page.get_physical_address());

                    add_page(page.get_physical_address(), page.get_virtual_address(), interval_count);
                    
                    csv.last()[2] = get_age_in_intervals(page.get_physical_address());
                    if (page.is_dirty()) {
                        write(page.get_physical_address());
                    } else {
                        no_write(page.get_physical_address());
                    }

                    csv.last()[4] = CSVString::format("0x%x", page.get_virtual_address());
                    csv.last()[5] = CSVString::format("0x%x", page.get_physical_address());

                    csv.last()[6] = was_modified_this_interval(page.get_physical_address());
                    csv.last()[7] = was_modified_last_interval(page.get_physical_address());
                    if (allocation.is_new()) {
                        add_new_objects(page.get_physical_address());
                    }
                    csv.last()[8] = has_new_objects(page.get_physical_address());
                    csv.last()[9] = get_write_count(page.get_physical_address());
                    csv.last()[10] = get_interval_since_last_write(page.get_physical_address());
                });
            });
            stack_infof("Site 0x%x complete\n", return_address);
        });

        age_all_pages();

        csv.write(file);
        csv.clear();
        stack_infof("Interval %d complete for page liveness\n", interval_count);
    }
    
    int get_first_interval_for_page(void* physical_address) {
        if (tracking.has(physical_address)) {
            return tracking.get(physical_address).first_interval;
        }
        return -1;
    }

    bool has_new_objects(void* physical_address) {
        if (tracking.has(physical_address)) {
            return tracking.get(physical_address).has_new_objects;
        }
        return false;
    }

    PageTracking &get_page_tracking(void* physical_address, void *virtual_address, int interval) {
        if (tracking.has(physical_address)) {
            return tracking.get(physical_address);
        }
        // If it doesn't exist, create it
        tracking.put(physical_address, PageTracking(interval, (uintptr_t)virtual_address, (uintptr_t)physical_address));
    }

    void age_all_pages() {
        tracking.map([&](void *ptr, PageTracking &page) {
            page.age();
        });
    }

    bool is_page_new(void *physical_address) {
        return !tracking.has(physical_address) || tracking.get(physical_address).first_interval == interval_count;
    }

    void add_page(void *physical_address, void *virtual_address, int interval) {
        if (!tracking.has(physical_address)) {
            tracking.put(physical_address, PageTracking(interval, (uintptr_t)virtual_address, (uintptr_t)physical_address));
        }
    }

    void write(void *physical_address) {
        if (tracking.has(physical_address)) {
            tracking.get(physical_address).write();
        }
    }

    void no_write(void *physical_address) {
        if (tracking.has(physical_address)) {
            tracking.get(physical_address).no_write();
        }
    }

    uint64_t get_age_in_intervals(void *physical_address) {
        if (tracking.has(physical_address)) {
            return tracking.get(physical_address).age_in_intervals;
        }
        return -1;
    }

    uint64_t get_interval_since_last_write(void *physical_address) {
        if (tracking.has(physical_address)) {
            return tracking.get(physical_address).age_since_last_write;
        }
        return -1;
    }

    uint64_t get_write_count(void *physical_address) {
        if (tracking.has(physical_address)) {
            return tracking.get(physical_address).write_count;
        }
        return -1;
    }

    void add_new_objects(void *physical_address) {
        if (tracking.has(physical_address)) {
            tracking.get(physical_address).add_new_objects();
        }
    }

    bool was_modified_this_interval(void *physical_address) {
        if (tracking.has(physical_address)) {
            return tracking.get(physical_address).modified_this_interval;
        }
        return false;
    }

    bool was_modified_last_interval(void *physical_address) {
        if (tracking.has(physical_address)) {
            return tracking.get(physical_address).modified_last_interval;
        }
        return false;
    }

    bool is_read_only_after_initial_write(void *physical_address) {
        if (tracking.has(physical_address)) {
            return tracking.get(physical_address).read_only_after_initial_write;
        }
        return false;
    }
};
