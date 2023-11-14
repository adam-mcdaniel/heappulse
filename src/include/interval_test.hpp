#pragma once
#include <stack_set.hpp>
#include <stack_vec.hpp>
#include <timer.hpp>
#include <backtrace.hpp>
#include <zlib.h>
// #include <fstream>
// #include <sstream>
// #include <iostream>
// #include <stdio.h>
#include <stack_io.hpp>
#include <stdlib.h>
#include <chrono>
#include <vector>
#include <signal.h>
#include <unistd.h>
#include <csetjmp>
#include <thread>
#include <sys/mman.h>
#include <cassert>
#include <condition_variable>
#include <dlfcn.h>
#include <execinfo.h>
#include <timer.hpp>
#include <bit_vec.hpp>

#define MPROTECT
#ifndef MPROTECT
#define PKEYS
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

class PageInfo {
public:
    PageInfo() : page_frame_number(0), start_address(NULL), end_address(NULL), read(false), write(false), exec(false), present(false), is_zero_page(false), dirty(false), soft_dirty(false) {}
    PageInfo(uint64_t page_frame_number, void* start_address, void* end_address, bool read, bool write, bool exec, bool is_zero_page, bool present, bool dirty, bool soft_dirty) : page_frame_number(page_frame_number), start_address(start_address), end_address(end_address), read(read), write(write), exec(exec), present(present), is_zero_page(is_zero_page), dirty(dirty), soft_dirty(soft_dirty) {}

    bool is_resident() const {
        return present && !is_zero_page;
    }

    bool is_absent() const {
        return !present;
    }

    void *get_virtual_address() const {
        return start_address;
    }

    void *get_physical_address() const {
        return (void*)(page_frame_number * PAGE_SIZE);
    }

    uint64_t get_page_frame_number() const {
        return page_frame_number;
    }

    uint64_t get_size_in_bytes() const {
        return (uint64_t)end_address - (uint64_t)start_address;
    }

    bool is_read() const {
        return read;
    }

    bool is_write() const {
        return write;
    }

    bool is_exec() const {
        return exec;
    }

    bool is_zero() const {
        return is_zero_page;
    }

    bool is_dirty() const {
        return dirty;
    }

    bool is_soft_dirty() const {
        return soft_dirty;
    }
private:
    uint64_t page_frame_number;
    void* start_address, *end_address;
    bool read, write, exec;
    bool is_zero_page;
    bool present;
    bool dirty, soft_dirty;
};

uint64_t count_virtual_pages(void *addr, uint64_t size_in_bytes) {
    return size_in_bytes / PAGE_SIZE + (size_in_bytes % PAGE_SIZE != 0);
}

static bool IS_PROTECTED = false;
// std::condition_variable protect_cv;

template<size_t Size>
bool get_page_info(void *addr, uint64_t size_in_bytes, StackVec<PageInfo, Size> &page_info, BitVec<Size> &present_pages) {
    bool protection = IS_PROTECTED;
    IS_PROTECTED = true;

    int pid = getpid();

    stack_logf("count_resident_pages(%p, %lu, %d)\n", addr, size_in_bytes, pid);

    // Make size_in_bytes a multiple of the page size
    uint64_t size_in_pages = count_virtual_pages(addr, size_in_bytes);
    size_in_bytes = size_in_pages * PAGE_SIZE;
    // Align the address to the page size
    addr = (void*)((uint64_t)addr / PAGE_SIZE * PAGE_SIZE);
    
    char filename[1024] = "";
    stack_sprintf<128>(filename, "/proc/%d/pagemap", pid);
    stack_logf("Filename: %s\n", filename);

    int fd = open(filename, O_RDONLY);
    if(fd < 0) {
        perror("open pagemap");
        return false;
    }

    int kpageflags_fd = open("/proc/kpageflags", O_RDONLY);
    if(kpageflags_fd < 0) {
        perror("open kflags");
        return false;
    }

    uint64_t start_address = (uint64_t)addr;
    uint64_t end_address = (uint64_t)((char*)addr + size_in_bytes);

    uint64_t n_resident_pages = 0;
    stack_logf("Size in pages: %\n", size_in_pages);
    size_t j = 0;
    for(uint64_t i = start_address; i < end_address; i+=PAGE_SIZE) {
        // uint64_t cur_page = (i - start_address) / PAGE_SIZE;

        uint64_t data;
        uint64_t index = (i / PAGE_SIZE) * sizeof(data);

        if(pread(fd, &data, sizeof(data), index) != sizeof(data)) {
            perror("pread");
            break;
        }
        
        // Present flag is in bit 63.
        if(!(data & (1ULL << 63))) {
            stack_logf("Ignoring absent page\n");
            j++;
            continue;
        }

        // File/shared is in bit 61.
        if(data & (1ULL << 61)) {
            stack_logf("Ignoring file page\n");
            j++;
            continue;
        }
        
        // Get page frame number
        uint64_t page_frame_number = data & 0x7FFFFFFFFFFFFFULL;

        uint64_t flags;
        uint64_t kpageflags_index = page_frame_number * sizeof(flags);

        // Check if is zero page using kpageflags
        if(pread(kpageflags_fd, &flags, sizeof(flags), kpageflags_index) != sizeof(flags)) {
            perror("pread");
            break;
        }

        // print_page_flags(i, page_frame_number, data, flags);

        // If is the zero page, continue.
        // Zero page flag is in bit 24.
        // if (flags & (1 << 24)) {
        //     printf("Ignoring zero page\n");
        //     continue;
        // }

        bool read = data & (1 << 2);
        bool write = data & (1 << 4);
        bool exec = data & (1 << 5);
        bool present = data & (1ULL << 63);
        bool soft_dirty = data & (1ULL << 55);
        bool is_zero_page = flags & (1 << 24);
        bool dirty = flags & (1 << 4);

        page_info.push(PageInfo(page_frame_number,
                                  (void*)i,
                                  (void*)(i + PAGE_SIZE),
                                  read,
                                  write,
                                  exec,
                                  is_zero_page,
                                  present,
                                  dirty,
                                  soft_dirty));
        present_pages.set(j++, present);
        n_resident_pages++;
        if (j >= Size) {
            break;
        }
        if (n_resident_pages >= size_in_pages) {
            break;
        }
        assert(n_resident_pages <= size_in_pages);
        assert(n_resident_pages == present_pages.count());
    }

    close(fd);
    close(kpageflags_fd);
    stack_logf("Done with count_resident_pages\n");

    IS_PROTECTED = protection;
    return true;
}

static uint64_t WORKING_THREAD_ID = 0;

bool is_working_thread() {
    return WORKING_THREAD_ID == (uint64_t)pthread_self();
}

void become_working_thread() {
    WORKING_THREAD_ID = (uint64_t)pthread_self();
    stack_logf("Became working thread with TID=%d\n", WORKING_THREAD_ID);
    IS_PROTECTED = true;
}

void no_longer_working_thread() {
    stack_logf("TID=%d is no longer working thread\n", WORKING_THREAD_ID);
    WORKING_THREAD_ID = 0;
    IS_PROTECTED = false;
}

// This is the handler for SIGSEGV. It's called when we try to access
// a protected page. This will put the thread to sleep until we finish
// compressing the page. This thread will then be woken up when we
// unprotect the page.
static void protection_handler(int sig, siginfo_t *si, void *unused)
{
    stack_logf("[handler] Got SIGSEGV at address: 0x%X\n", (uint64_t)si->si_addr);
    // std::cout << "Got SIGSEGV at address: 0x" << std::hex << si->si_addr << std::endl;
    // char buf[1024];
    // sprintf(buf, "Got SIGSEGV at address: 0x%lx\n", (long) si->si_addr);
    // write(STDOUT_FILENO, buf, strlen(buf));

    // Is this thread the main?
    if (is_working_thread()) {
        // sprintf(buf, "[FAULT] Working thread, giving back access: 0x%lx\n", (long) si->si_addr);
        // write(STDOUT_FILENO, buf, strlen(buf));

        long page_size = sysconf(_SC_PAGESIZE);
        void* aligned_address = (void*)((uint64_t)si->si_addr & ~(page_size - 1));
        mprotect(aligned_address, getpagesize(), PROT_READ | PROT_WRITE | PROT_EXEC);
        stack_logf("[handler] Giving back access to 0x%X\n", (uint64_t)si->si_addr);
    } else {
        if (!IS_PROTECTED) {
            // sprintf(buf, "[FAULT] Dereferenced address 0x%lx when unprotected, halting program\n", (long) si->si_addr);
            // write(STDOUT_FILENO, buf, strlen(buf));
            stack_logf("[FAULT] Dereferenced address 0x%X when unprotected, halting program\n", (uint64_t)si->si_addr);
            usleep(250000);
        }
        stack_logf("[INFO] Caught access of temporarily protected memory: 0x%X\n", (uint64_t)si->si_addr);
        // sprintf(buf, "[INFO] Caught access of temporarily protected memory: 0x%lx\n", (long) si->si_addr);
        // write(STDOUT_FILENO, buf, strlen(buf));
        while (IS_PROTECTED) {}
        stack_logf("[handler] Giving back access to 0x%X\n", (uint64_t)si->si_addr);

        // sprintf(buf, "[INFO] Resuming after protection ended: 0x%lx\n", (long) si->si_addr);
        // write(STDOUT_FILENO, buf, strlen(buf));
    }
}

// This sets the SEGV signal handler to be the protection handler.
// The protection handler will capture the segfault caused by mprotect,
// and keep the program blocked until the compression tests are done
void setup_protection_handler()
{
    stack_logf("Setting up protection handler\n");
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = protection_handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;

    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    } else {
        stack_logf("Set up protection handler\n");
    }
}

const int TRACKED_ALLOCATIONS_PER_SITE = 1000;
const int TRACKED_ALLOCATION_SITES = 1000;
const int TOTAL_TRACKED_ALLOCATIONS = TRACKED_ALLOCATIONS_PER_SITE * TRACKED_ALLOCATION_SITES;

static int PKEY = -1;
static bool PKEY_INITIALIZED = false;

struct Allocation {
    void *ptr;
    size_t size;
    Backtrace backtrace;

    void log() const {
        stack_logf("Allocation: %p, size: %x\n", ptr, size);
    }

    void protect() {
        IS_PROTECTED = true;
        #ifdef MPROTECT
        protect_with_mprotect();
        #endif
        #ifdef PKEYS
        protect_with_pkeys();
        pkey_protect();
        #endif
    }

    void unprotect() {
        #ifdef MPROTECT
        unprotect_with_mprotect();
        #endif
        #ifdef PKEYS
        pkey_unprotect();
        #endif
        IS_PROTECTED = false;
    }

    void protect_with_mprotect() {
        long page_size = sysconf(_SC_PAGESIZE);
        uintptr_t address = (uintptr_t)ptr;
        void* aligned_address = (void*)(address & ~(page_size - 1));

        // Align the size to the page boundary
        size_t aligned_size = (size + page_size - 1) & ~(page_size - 1);

        if (mprotect(aligned_address, aligned_size, PROT_READ) == -1) {
            perror("mprotect");
            exit(1);
        }
    }

    void unprotect_with_mprotect() {
        long page_size = sysconf(_SC_PAGESIZE);
        uintptr_t address = (uintptr_t)ptr;
        void* aligned_address = (void*)(address & ~(page_size - 1));

        // Align the size to the page boundary
        size_t aligned_size = (size + page_size - 1) & ~(page_size - 1);

        if (mprotect(aligned_address, aligned_size, PROT_READ | PROT_WRITE | PROT_EXEC) == -1) {
            perror("mprotect");
            exit(1);
        }
    }

    void protect_with_pkeys() {
        if (!PKEY_INITIALIZED) {
            PKEY = pkey_alloc(0, 0);
            if (PKEY == -1) {
                perror("pkey_alloc");
                exit(1);
            }
            PKEY_INITIALIZED = true;
        }
        long page_size = sysconf(_SC_PAGESIZE);
        uintptr_t address = (uintptr_t)ptr;
        void* aligned_address = (void*)(address & ~(page_size - 1));

        // Align the size to the page boundary
        size_t aligned_size = (size + page_size - 1) & ~(page_size - 1);

        if (pkey_mprotect(aligned_address, aligned_size, PROT_READ, 0) == -1) {
            perror("pkey_mprotect");
            exit(1);
        }
    }

    void pkey_protect() {
        if (!PKEY_INITIALIZED) {
            PKEY = pkey_alloc(0, 0);
            if (PKEY == -1) {
                perror("pkey_alloc");
                exit(1);
            }
            PKEY_INITIALIZED = true;
        }
        long page_size = sysconf(_SC_PAGESIZE);
        uintptr_t address = (uintptr_t)ptr;
        void* aligned_address = (void*)(address & ~(page_size - 1));

        // Align the size to the page boundary
        size_t aligned_size = (size + page_size - 1) & ~(page_size - 1);

        if (pkey_set(PKEY, PKEY_DISABLE_ACCESS) == -1) {
            perror("pkey_set");
            exit(1);
        }
    }

    void pkey_unprotect() {
        if (!PKEY_INITIALIZED) {
            PKEY = pkey_alloc(0, 0);
            if (PKEY == -1) {
                perror("pkey_alloc");
                exit(1);
            }
            PKEY_INITIALIZED = true;
        }
        long page_size = sysconf(_SC_PAGESIZE);
        uintptr_t address = (uintptr_t)ptr;
        void* aligned_address = (void*)(address & ~(page_size - 1));

        // Align the size to the page boundary
        size_t aligned_size = (size + page_size - 1) & ~(page_size - 1);

        if (pkey_set(PKEY, 0) == -1) {
            perror("pkey_set");
            exit(1);
        }
    }

    void unprotect_with_pkeys() {
        long page_size = sysconf(_SC_PAGESIZE);
        uintptr_t address = (uintptr_t)ptr;
        void* aligned_address = (void*)(address & ~(page_size - 1));

        // Align the size to the page boundary
        size_t aligned_size = (size + page_size - 1) & ~(page_size - 1);

        if (pkey_mprotect(aligned_address, aligned_size, PROT_READ | PROT_WRITE | PROT_EXEC, 0) == -1) {
            perror("pkey_mprotect");
            exit(1);
        }
    }

// private:
    template<size_t Size>
    BitVec<Size> present_pages() {
        BitVec<Size> present_pages;
        StackVec<PageInfo, Size> page_info;


        if (get_page_info<Size>(ptr, size, page_info, present_pages)) {
            return present_pages;
        } else {
            stack_logf("Unable to get page info\n");
            exit(1);
        }
    }

    template<size_t Size>
    StackVec<PageInfo, Size> page_info() {
        BitVec<Size> present_pages;
        StackVec<PageInfo, Size> page_info;

        if (get_page_info<Size>(ptr, size, page_info, present_pages)) {
            return page_info;
        } else {
            stack_logf("Unable to get page info\n");
            exit(1);
        }
    }
};
struct AllocationSite {
    uintptr_t return_address;
    StackMap<void*, Allocation, TRACKED_ALLOCATIONS_PER_SITE> allocations;

    void log() const {
        stack_logf("AllocationSite: %p\n", return_address);
        for (int i=0; i<allocations.size(); i++) {
            stack_logf("   ");
            allocations.nth_entry(i).value.log();
        }
    }
};

struct IntervalTestConfig {
    double period_milliseconds = 10000.0;
};

class IntervalTest {
public:

    // A virtual method for setting up the test
    // This is run once on startup. Do your initialization here
    virtual void setup() {}
    // A virtual method for cleaning up the test
    // This is run once on shutdown. Do your cleanup here
    virtual void cleanup() {}

    // A virtual method for running the test's interval
    virtual void interval(
        const StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> &allocation_sites,
        const StackVec<Allocation, TOTAL_TRACKED_ALLOCATIONS> &allocations
    ) {}

    virtual void schedule() {}

    ~IntervalTest() {
        cleanup();
    }
};


static bool IS_IN_TEST = false;

class IntervalTestSuite {
    StackVec<IntervalTest*, 10> tests;
    void heart_beat() {
        if (second_timer.has_elapsed(1000)) {
            stack_infof("Heart beat (1 second has elapsed)\n");
            second_timer.reset();
        }
    }

public:
    IntervalTestSuite() {
        setup_protection_handler();
    }

    void add_test(IntervalTest *test) {
        // hook_lock.lock();
        test->setup();
        tests.push(test);
        assert(tests.size() > 0);
        // hook_lock.unlock();
    }


    void update(void *ptr, size_t size, uintptr_t return_address) {
        heart_beat();

        if (IS_PROTECTED || IS_IN_TEST) {
            return;
        }
        
        stack_debugf("IntervalTestSuite::update\n");
        // if (!hook_lock.try_lock()) {
        //     stack_debugf("Unable to lock hook\n");
        //     return;
        // }

        AllocationSite site;
        if (allocation_sites.has(return_address)) {
            site = allocation_sites.get(return_address);
        } else {
            site = {return_address, StackMap<void*, Allocation, TRACKED_ALLOCATIONS_PER_SITE>()};
        }

        stack_debugf("Allocation at %X, size: %d\n", ptr, size);
        stack_debugf("Return address: %X\n", return_address);
        stack_debugf("Allocation-site bookkeeping elements: %d\n", site.allocations.num_entries());
        stack_debugf("Allocation-sites: %d\n", allocation_sites.num_entries());

        Allocation allocation = {ptr, size, Backtrace::capture()};
        if (site.allocations.has(ptr)) {
            site.allocations.put(ptr, allocation);
        } else if (!site.allocations.full()) {
            site.allocations.put(ptr, allocation);
        } else {
            stack_debugf("Unable to add allocation to site\n");
            // hook_lock.unlock();
        }

        if (site.allocations.full()) {
            stack_debugf("Allocation-site bookkeeping elements: %d\n", site.allocations.num_entries());
            stack_debugf("Allocation-sites: %d\n", allocation_sites.num_entries());
            stack_debugf("Unable to add allocation to site\n");
            // hook_lock.unlock();
            return;
        }
        // try {
        //     site.allocations.put(ptr, allocation);
        // } catch (const std::exception& e) {
        //     stack_debugf("Unable to add allocation to site\n");
        //     // hook_lock.unlock();
        //     return;
        // }

        if (allocation_sites.has(return_address)) {
            allocation_sites.put(return_address, site);
        } else if (!allocation_sites.full()) {
            allocation_sites.put(return_address, site);
        } else {
            // stack_debugf("Unable to add allocation to site\n");
            stack_debugf("Unable to add allocation site\n");
            // hook_lock.unlock();
        }

        // try {
        //     allocation_sites.put(return_address, site);
        // } catch (const std::exception& e) {
        //     stack_debugf("Unable to add allocation site\n");
        //     // hook_lock.unlock();
        //     // schedule();
        //     return;
        //     // stack_debugf("Unable to add allocation site\n");
        //     // // Evict the allocation site with the least number of allocations
        //     // uintptr_t min_return_address = 0;
        //     // size_t min_num_allocations = SIZE_MAX;
        //     // StackVec<uintptr_t, TRACKED_ALLOCATION_SITES> keys;
        //     // allocation_sites.keys(keys);

        //     // for (size_t i=0; i<keys.size() && i <TRACKED_ALLOCATION_SITES; i++) {
        //     //     uintptr_t key = keys[i];
        //     //     if (!allocation_sites.has(key)) {
        //     //         continue;
        //     //     }
        //     //     AllocationSite site = allocation_sites.get(key);
        //     //     if (site.allocations.size() < min_num_allocations) {
        //     //         min_return_address = key;
        //     //         min_num_allocations = site.allocations.size();
        //     //     }
        //     // }

        //     // if (min_return_address == 0) {
        //     //     stack_debugf("Unable to evict allocation site\n");
        //     //     // hook_lock.unlock();
        //     //     return;
        //     // }

        //     // stack_debugf("Evicting allocation site: %X, which tracked %d allocations\n", min_return_address, min_num_allocations);
        //     // if (allocation_sites.has(min_return_address)) {
        //     //     allocation_sites.remove(min_return_address);
        //     // }
        //     // allocation_sites.put(return_address, site);
        // }
        // hook_lock.unlock();
        
        schedule();
        
        stack_debugf("Leaving IntervalTestSuite::update\n");
    }

    bool contains(void *ptr) {
        for (size_t i=0; i<allocation_sites.size(); i++) {
            if (allocation_sites.nth_entry(i).occupied) {
                AllocationSite site = allocation_sites.nth_entry(i).value;
                if (site.allocations.has(ptr)) {
                    return true;
                }
            }
        }
        return false;
    }

    void invalidate(void *ptr) {
        heart_beat();
        stack_debugf("IntervalTestSuite::invalidate\n");
        stack_debugf("Invalidating %X\n", ptr);
        allocations.clear();
        for (size_t i=0; i<allocation_sites.size(); i++) {
            if (allocation_sites.nth_entry(i).occupied) {
                AllocationSite site = allocation_sites.nth_entry(i).value;
                if (site.allocations.has(ptr)) {
                    // hook_lock.lock();
                    site.allocations.remove(ptr);
                    allocation_sites.put(site.return_address, site);
                    // hook_lock.unlock();
                }
            }
        }

        schedule();
        stack_debugf("Leaving IntervalTestSuite::invalidate\n");
    }

    ~IntervalTestSuite() {
        cleanup();
    }
private:
    void schedule() {
        heart_beat();
        stack_debugf("IntervalTestSuite::schedule\n");
        if (IS_IN_TEST) {
            return;
        }
        IS_IN_TEST = true;
        // schedule_lock.lock();
        // hook_lock.lock();

        if (timer.elapsed_milliseconds() > config.period_milliseconds) {
            stack_debugf("Starting test\n");

            stack_logf("Starting interval\n");
            timer.reset();
            stack_logf("Getting allocations\n");
            get_allocs();
            stack_logf("Starting interval\n");
            interval();
            timer.reset();
            stack_logf("Finished interval\n");
            stack_debugf("Done with test\n");
            if (IS_IN_TEST) {
                IS_IN_TEST = false;
            }
        } else {
            stack_debugf("Only %fms have elapsed, not yet at %fms interval\n", timer.elapsed_milliseconds(), config.period_milliseconds);
        }
        // schedule_lock.unlock();
        // hook_lock.unlock();
        stack_debugf("IntervalTestSuite::schedule\n");
    }

    void interval() {
        stack_logf("IntervalTestSuite::interval\n");
        become_working_thread();

        static std::mutex interval_lock;
        std::lock_guard<std::mutex> lock(interval_lock);
        // interval_lock.lock();
        for (size_t i=0; i<tests.size(); i++) {
            stack_logf("Running interval for test %d\n", i);
            tests[i]->interval(allocation_sites, allocations);
        }
        // hook_lock.unlock();
        no_longer_working_thread();
        stack_logf("Finished interval\n");
    }

    // void setup() {
    //     stack_logf("IntervalTestSuite::setup\n");
    //     if (is_setup) {
    //         return;
    //     }
    //     for (size_t i=0; i<tests.size(); i++) {
    //         tests[i]->setup();
    //     }
    //     is_setup = true;
    // }

    void cleanup() {
        for (size_t i=0; i<tests.size(); i++) {
            tests[i]->cleanup();
        }
    }

    void get_allocs() {
        // hook_lock.lock();
        allocations.clear();
        for (size_t i=0; i<allocation_sites.size(); i++) {
            if (allocation_sites.nth_entry(i).occupied) {
                allocation_sites.nth_entry(i).value.allocations.values(allocations);
            }
        }
        allocations.sort_by([](const Allocation& a, const Allocation& b) {
            return (uintptr_t)a.ptr < (uintptr_t)b.ptr;
        });
        // hook_lock.unlock();
    }

    Timer timer, second_timer;
    IntervalTestConfig config;

    StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> allocation_sites;
    StackVec<Allocation, TOTAL_TRACKED_ALLOCATIONS> allocations;


    // This is used to protect the main thread while we're compressing
    // static std::mutex hook_lock;
    // static std::mutex schedule_lock;

    bool is_setup = false;
};