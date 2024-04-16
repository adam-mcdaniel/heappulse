#pragma once

#include <stack_set.hpp>
#include <stack_vec.hpp>
#include <timer.hpp>
#include <backtrace.hpp>
#include <zlib.h>
#include <stack_io.hpp>
#include <stdint.h>
#include <stdlib.h>
#include <chrono>
#include <vector>
#include <signal.h>
#include <unistd.h>
#include <ucontext.h>
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
    PageInfo(uint64_t page_frame_number, void* start_address, void* end_address, bool read, bool write, bool exec, bool is_zero_page, bool present, bool dirty, bool soft_dirty, bool file_mapped, bool referenced) : page_frame_number(page_frame_number), start_address(start_address), end_address(end_address), read(read), write(write), exec(exec), present(present), is_zero_page(is_zero_page), dirty(dirty), soft_dirty(soft_dirty), file_mapped(file_mapped), referenced(referenced) {}

    /// @brief Get the size of the page in bytes
    /// @return The size of the page in bytes
    size_t size() const {
        return PAGE_SIZE;
    }

    void set_page_frame_number(uint64_t page_frame_number) {
        this->page_frame_number = page_frame_number;
    }

    void set_start_address(void* start_address) {
        this->start_address = start_address;
    }

    void set_end_address(void* end_address) {
        this->end_address = end_address;
    }

    void set_read(bool read) {
        this->read = read;
    }

    void set_write(bool write) {
        this->write = write;
    }

    void set_exec(bool exec) {
        this->exec = exec;
    }

    void set_present(bool present) {
        this->present = present;
    }

    void set_is_zero_page(bool is_zero_page) {
        this->is_zero_page = is_zero_page;
    }

    void set_dirty(bool dirty) {
        this->dirty = dirty;
    }

    void set_soft_dirty(bool soft_dirty) {
        this->soft_dirty = soft_dirty;
    }

    void set_is_file_mapped(bool is_file_mapped) {
        this->file_mapped = is_file_mapped;
    }

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
        return soft_dirty;
    }

    bool has_been_written() const {
        return soft_dirty;
    }

    bool has_been_read() const {
        return referenced;
    }

    bool is_soft_dirty() const {
        return soft_dirty;
    }

    bool is_file_mapped() const {
        return file_mapped;
    }
private:
    uint64_t page_frame_number;
    void* start_address, *end_address;
    bool read, write, exec;
    bool present;
    bool is_zero_page;
    bool dirty, soft_dirty;
    bool file_mapped;
    bool referenced;
};

uint64_t count_virtual_pages(void *addr, uint64_t size_in_bytes) {
    return size_in_bytes / PAGE_SIZE + (size_in_bytes % PAGE_SIZE != 0);
}

static bool IS_PROTECTED = false;
// std::condition_variable protect_cv;

/// Clear the soft dirty bits for the program's pages.
void perform_clear_soft_dirty_bits() {
    stack_debugf("Clearing soft dirty bits\n");
    bool protection = IS_PROTECTED;
    IS_PROTECTED = true;

    static bool is_open = false;
    static int fd = -1;
    if (!is_open) {

        int pid = getpid();
        char filename[1024] = "";
        stack_sprintf<128>(filename, "/proc/%d/clear_refs", pid);

        // Open the clear_refs file
        // Only open it ONCE statically
        fd = open(filename, O_WRONLY);
        is_open = true;
    }

    if(fd < 0) {
        perror("open clear_refs");
        return;
    }

    // Write `4` to the clear_refs file to clear the soft dirty bits
    if(write(fd, "1", 1) != 1) {
        perror("write clear_refs");
        return;
    }
    // Write `4` to the clear_refs file to clear the soft dirty bits
    if(write(fd, "2", 1) != 1) {
        perror("write clear_refs");
        return;
    }
    // Write `4` to the clear_refs file to clear the soft dirty bits
    if(write(fd, "3", 1) != 1) {
        perror("write clear_refs");
        return;
    }
    // Write `4` to the clear_refs file to clear the soft dirty bits
    if(write(fd, "4", 1) != 1) {
        perror("write clear_refs");
        return;
    }
    // // Clear the reference bits
    // if(write(fd, "2", 1) != 1) {
    //     perror("write clear_refs");
    //     return;
    // }
    // if (write(fd, "1", 1) != 1) {
    //     perror("write clear_refs");
    //     return;
    // }

    // int fd = open(filename, O_WRONLY);
    // if(fd < 0) {
    //     perror("open clear_refs");
    //     return;
    // }

    // // Write `4` to the clear_refs file to clear the soft dirty bits
    // if(write(fd, "4", 1) != 1) {
    //     perror("write clear_refs");
    //     return;
    // }
    // close(fd);

    IS_PROTECTED = protection;
}

template<size_t Size>
bool get_page_info(void *addr, uint64_t size_in_bytes, StackVec<PageInfo, Size> &page_info, BitVec<Size> &present_pages, std::function<bool(const PageInfo&)> filter) {
    bool protection = IS_PROTECTED;
    IS_PROTECTED = true;

    int pid = getpid();

    stack_debugf("count_resident_pages(%p, %lu, %d)\n", addr, size_in_bytes, pid);

    // Make size_in_bytes a multiple of the page size
    uint64_t size_in_pages = count_virtual_pages(addr, size_in_bytes);
    size_in_bytes = size_in_pages * PAGE_SIZE;
    // Align the address to the page size
    addr = (void*)((uint64_t)addr / PAGE_SIZE * PAGE_SIZE);

    static bool is_open = false;
    static int pagemap_fd = -1;
    static int kpageflags_fd = -1;
    
    if (!is_open) {
        char filename[1024] = "";
        stack_sprintf<128>(filename, "/proc/%d/pagemap", pid);
        stack_logf("Filename: %s\n", filename);

        kpageflags_fd = open("/proc/kpageflags", O_RDONLY);
        pagemap_fd = open(filename, O_RDONLY);

        is_open = true;
    }

    if(pagemap_fd < 0) {
        perror("open pagemap");
        return false;
    }

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

        if(pread(pagemap_fd, &data, sizeof(data), index) != sizeof(data)) {
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
        bool is_file_mapped = data & (1ULL << 61);
        bool present = data & (1ULL << 63);
        bool soft_dirty = data & (1ULL << 55);
        bool is_zero_page = flags & (1 << 24);
        bool dirty = flags & (1 << 4);
        // The page has been referenced since the last time it was checked
        // This is the 3rd bit in the flags
        bool referenced = flags & (1 << 3);
        PageInfo page = PageInfo(page_frame_number,
                                  (void*)i,
                                  (void*)(i + PAGE_SIZE),
                                  read,
                                  write,
                                  exec,
                                  is_zero_page,
                                  present,
                                  dirty,
                                  soft_dirty,
                                  is_file_mapped,
                                  referenced);
        if (filter(page)) {
            page_info.push(page);
            present_pages.set(j++, present);
        }
        n_resident_pages++;
        if (page_info.full()) {
            stack_warnf("Page info full: filtered pages=%d/%d\n", page_info.size(), page_info.max_size());
            break;
        }
        if (n_resident_pages >= size_in_pages) {
            break;
        }
        assert(n_resident_pages <= size_in_pages);
        // assert(n_resident_pages == present_pages.count());
    }

    // close(pagemap_fd);
    // close(kpageflags_fd);
    stack_debugf("Done with count_resident_pages\n");

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
static void protection_handler(int sig, siginfo_t *si, void *ucontext);

// This sets the SEGV signal handler to be the protection handler.
// The protection handler will capture the segfault caused by mprotect,
// and keep the program blocked until the compression tests are done
void setup_protection_handler()
{
    stack_logf("Setting up protection handler\n");
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGSEGV);
    sigaddset(&sa.sa_mask, SIGILL);
    sigaddset(&sa.sa_mask, SIGBUS);
    sigaddset(&sa.sa_mask, SIGFPE);
    sigprocmask(SIG_UNBLOCK, &sa.sa_mask, NULL);
    // Allow the signal to be received by the thread that caused the fault
    sa.sa_sigaction = protection_handler;

    // sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESTART | SA_NODEFER;
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;

    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    } else {
        stack_logf("Set up protection handler\n");
    }
    if (sigaction(SIGILL, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    } else {
        stack_logf("Set up protection handler\n");
    }
    if (sigaction(SIGBUS, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    } else {
        stack_logf("Set up protection handler\n");
    }
    if (sigaction(SIGFPE, &sa, NULL) == -1) {
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
    /// @brief The pointer to the allocation
    void *ptr;
    /// @brief The size of the allocation
    size_t size;
    #ifdef COLLECT_BACKTRACE
    /// @brief The backtrace of the call stack that made the allocation
    Backtrace backtrace;
    #endif
    /// @brief The time the allocation was made
    std::chrono::steady_clock::time_point allocation_time;
    /// @brief The age of the allocation in intervals
    size_t age = 0;

    Allocation() : ptr(NULL), size(0) {
        allocation_time = std::chrono::steady_clock::now();
    }

    Allocation(void *ptr, size_t size) : ptr(ptr), size(size) {
        allocation_time = std::chrono::steady_clock::now();
    }

    bool operator==(const Allocation &other) const {
        return ptr == other.ptr;
    }

    // @brief Check if the allocation contains the address
    /// @param address The address to check
    /// @return True if the allocation contains the address, false otherwise
    bool contains(void *address) const {
        return address >= ptr && address < (void*)((uint64_t)ptr + size);
    }

    /// @brief Check if the allocation is new (allocated in the current interval)
    /// @return True if the allocation is new, false otherwise
    bool is_new() const {
        return age == 0;
    }

    /// @brief Get the age of the pointer in intervals
    /// @return The age of the pointer in intervals
    size_t get_age() const {
        return age;
    }

    /// @brief Tick the age of the allocation by one interval
    void tick_age() {
        age++;
    }

    /// @brief Check if the allocation has any dirty pages
    /// @return True if the allocation has any dirty pages, false otherwise
    bool is_dirty() {
        // Get the page info
        auto pages = physical_pages<10000>();
        for (size_t i=0; i<pages.size(); i++) {
            if (pages[i].is_dirty()) {
                return true;
            }
        }
        return false;
    }

    /// @brief Get the time the allocation was made
    /// @return The time the allocation was made
    std::chrono::steady_clock::time_point get_time_allocated() const {
        return allocation_time;
    }
    
    /// @brief Get the time the allocation was made in milliseconds (since epoch)
    /// @return The time the allocation was made in milliseconds (since epoch)
    uint64_t get_time_allocated_ms() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(allocation_time.time_since_epoch()).count();
    }

    void log() const {
        stack_logf("Allocation: %p, size: %x\n", ptr, size);
    }

    void protect(uint64_t protections=PROT_NONE) {
        #ifdef MPROTECT
        protect_with_mprotect(protections);
        #endif
        #ifdef PKEYS
        protect_with_pkeys(protections);
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
    }

    void protect_with_mprotect(uint64_t protections) {
        long page_size = sysconf(_SC_PAGESIZE);
        uintptr_t address = (uintptr_t)ptr;
        void* aligned_address = (void*)(address & ~(page_size - 1));

        // Align the size to the page boundary
        size_t aligned_size = (size + page_size - 1) & ~(page_size - 1);
        stack_debugf("Protecting %p, size: %d\n", aligned_address, aligned_size);

        if (mprotect(aligned_address, aligned_size, protections) == -1) {
            perror("mprotect");
            exit(1);
        }
    }

    void unprotect_with_mprotect() {
        protect_with_mprotect(PROT_READ | PROT_WRITE | PROT_EXEC);
    }

    void protect_with_pkeys(uint64_t protections) {
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

        if (pkey_mprotect(aligned_address, aligned_size, protections, 0) == -1) {
            perror("pkey_mprotect");
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

    /*
    void pkey_protect() {
        if (!PKEY_INITIALIZED) {
            PKEY = pkey_alloc(0, 0);
            if (PKEY == -1) {
                perror("pkey_alloc");
                exit(1);
            }
            PKEY_INITIALIZED = true;
        }
        
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
        
        if (pkey_set(PKEY, 0) == -1) {
            perror("pkey_set");
            exit(1);
        }
    }
    */

// private:
    template<size_t Size>
    BitVec<Size> present_pages() {
        BitVec<Size> present_pages;
        StackVec<PageInfo, Size> page_info;


        if (get_page_info<Size>(ptr, size, page_info, present_pages)) {
            return present_pages;
        } else {
            stack_errorf("Unable to get page info\n");
            exit(1);
        }
    }

    // template<size_t Size>
    // StackVec<PageInfo, Size> page_info() {
    //     BitVec<Size> present_pages;
    //     StackVec<PageInfo, Size> page_info;

    //     if (get_page_info<Size>(ptr, size, page_info, present_pages, [](const PageInfo& page) { return page.is_resident(); })) {
    //         return page_info;
    //     } else {
    //         stack_errorf("Unable to get page info\n");
    //         exit(1);
    //     }
    // }

    template<size_t Size>
    StackVec<PageInfo, Size> physical_pages(bool non_zero=true) {
        BitVec<Size> present_pages;
        StackVec<PageInfo, Size> physical_pages;

        if (get_page_info<Size>(ptr, size, physical_pages, present_pages, [&](const PageInfo& page) { return page.is_resident() && (!non_zero || !page.is_zero()); })) {
            // StackVec<PageInfo, Size> physical_pages;
            // for (size_t i=0; i<page_info.size(); i++) {
            //     if (present_pages[i]) {
            //         physical_pages.push(page_info[i]);
            //     }
            // }
            return physical_pages;
        } else {
            stack_errorf("Unable to get page info\n");
            exit(1);
        }
    }

    template<size_t Size>
    StackVec<PageInfo, Size> physical_pages(std::function<bool(const PageInfo&)> filter) {
        BitVec<Size> present_pages;
        StackVec<PageInfo, Size> physical_pages;

        if (get_page_info<Size>(ptr, size, physical_pages, present_pages, filter)) {
            // StackVec<PageInfo, Size> physical_pages;
            // for (size_t i=0; i<page_info.size(); i++) {
            //     if (present_pages[i]) {
            //         physical_pages.push(page_info[i]);
            //     }
            // }
            return physical_pages;
        } else {
            stack_errorf("Unable to get page info\n");
            exit(1);
        }
    }
};
struct AllocationSite {
    uintptr_t return_address;
    StackMap<void*, Allocation, TRACKED_ALLOCATIONS_PER_SITE> allocations;

    void log() const {
        stack_logf("AllocationSite: %p\n", return_address);
        // for (size_t i=0; i<allocations.max_size(); i++) {
        //     stack_logf("   ");
        //     allocations.nth_entry(i).value.log();
        // }
        allocations.map([](auto key, Allocation const &allocation) {
            stack_logf("   ");
            allocation.log();
        });
    }

    void tick_age() {
        // for (size_t i=0; i<allocations.max_size(); i++) {
        //     allocations.nth_entry(i).value.tick_age();
        // }
        allocations.map([](void *key, Allocation &allocation) {
            allocation.tick_age();
        });
    }
};

// Implement hashing for allocation
namespace std {
    template <>
    struct hash<Allocation> {
        std::size_t operator()(const Allocation &alloc) const {
            return std::hash<void*>()(alloc.ptr);
        }
    };
}

/// A map to track pages that incur a page fault.
/// This is used to track which pages incur a write between intervals.
static StackSet<void*, MAX_TRACKED_ACCESSES> written_addresses, read_addresses;


StackVec<void*, MAX_TRACKED_ACCESSES> get_written_addresses() {
    return written_addresses.items();
}

StackVec<void*, MAX_TRACKED_ACCESSES> get_read_addresses() {
    return read_addresses.items();
}

void mark_address_written(void *address) {
    written_addresses.insert(address);
}

void mark_address_read(void *address) {
    read_addresses.insert(address);
}

void clear_accesses() {
    read_addresses.clear();
    written_addresses.clear();
}

struct IntervalTestConfig {
    double period_milliseconds = 5000.0;
    bool clear_soft_dirty_bits = true;
};


class IntervalTest {
private:
    bool is_quitting = false;
public:
    IntervalTest() : is_quitting(false) {}

    // This is called by the test when it no longer needs to be run
    void quit() {
        stack_infof("Quitting %s\n", name());
        is_quitting = true;
    }

    bool has_quit() const {
        return is_quitting;
    }

    virtual const char *name() const {
        return "Base IntervalTest";
    }

    // A virtual method that gets called when an allocation is made
    virtual void on_alloc(const Allocation &alloc) {}
    // A virtual method that gets called when an allocation is freed
    virtual void on_free(const Allocation &alloc) {}
    // A virtual method that gets called when an allocation is accessed
    virtual void on_access(const Allocation &alloc, bool is_write) {}
    // A virtual method that gets called when an allocation is written to
    virtual void on_write(const Allocation &alloc) {}
    // A virtual method that gets called when an allocation is read from
    virtual void on_read(const Allocation &alloc) {}

    // A virtual method for setting up the test
    // This is run once on startup. Do your initialization here
    virtual void setup() {}
    // A virtual method for cleaning up the test
    // This is run once on shutdown. Do your cleanup here
    virtual void cleanup() {}

    // A virtual method for running the test's interval
    virtual void interval(
        const StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> &allocation_sites
    ) {}

    virtual void schedule() {}

    ~IntervalTest() {
        cleanup();
    }
};


class IntervalTestSuite {
public:
    IntervalTestSuite() {
        setup_protection_handler();
        if (config.clear_soft_dirty_bits) {
            stack_warnf("Clearing soft dirty bits\n");
            perform_clear_soft_dirty_bits();
        }
    }

    IntervalTestSuite(IntervalTestConfig config) {
        this->config = config;
        setup_protection_handler();
        if (config.clear_soft_dirty_bits) {
            stack_warnf("Clearing soft dirty bits\n");
            perform_clear_soft_dirty_bits();
        }
    }

    bool can_update() const {
        return !IS_PROTECTED && !is_done() && !is_in_interval;
    }

    void add_test(IntervalTest *test) {
        test->setup();
        tests.push(test);
        assert(tests.size() > 0);
    }

    void protect_allocations() {
        #ifdef GUARD_ACCESSES
        setup_protection_handler();
        allocation_sites.map([](uintptr_t key, AllocationSite &site) {
            stack_infof("Protecting allocation site %X\n", site.return_address);
            site.allocations.map([](void *key, Allocation &alloc) {
                alloc.protect(PROT_NONE);

                // void *addr = alloc.ptr;
                // void *aligned_addr = (void*)((uint64_t)addr & ~(PAGE_SIZE - 1));
                // size_t size = PAGE_SIZE;
                // stack_infof("Protecting %p, size: %d\n", aligned_addr, size);
                // if (mprotect(aligned_addr, size, PROT_NONE) == -1) {
                //     perror("mprotect");
                //     exit(1);
                // }
            });
        });
        stack_infof("Protected allocations\n");
        #endif
    }

    void update(void *ptr, size_t size, uintptr_t return_address) {
        stack_debugf("IntervalTestSuite::update\n");
        stack_debugf("Got pointer: %p\n", ptr);
        heart_beat();

        setup_protection_handler();

        if (IS_PROTECTED || !can_update()) {
            return;
        }

        if (!hook_lock.try_lock()) {
            stack_debugf("Unable to lock hook\n");
            return;
        } else {
            stack_debugf("Locked hook\n");
        }

        AllocationSite site;
        if (allocation_sites.has(return_address)) {
            site = allocation_sites.get(return_address);
        } else {
            site = {return_address, StackMap<void*, Allocation, TRACKED_ALLOCATIONS_PER_SITE>()};
        }

        stack_debugf("Allocation at %X, size: %d\n", (uintptr_t)ptr, size);
        stack_debugf("Return address: %X\n", return_address);
        stack_debugf("Allocation-site bookkeeping elements: %d\n", site.allocations.num_entries());
        stack_debugf("Allocation-sites: %d\n", allocation_sites.num_entries());

        Allocation allocation = Allocation(ptr, size);
        if (site.allocations.has(ptr)) {
            site.allocations.put(ptr, allocation);
        } else if (!site.allocations.full()) {
            site.allocations.put(ptr, allocation);
        } else {
            stack_debugf("Unable to add allocation to site\n");
        }

        if (site.allocations.full()) {
            stack_debugf("Allocation-site bookkeeping elements: %d\n", site.allocations.num_entries());
            stack_debugf("Allocation-sites: %d\n", allocation_sites.num_entries());
            stack_debugf("Unable to add allocation to site\n");
            stack_debugf("Releasing lock\n");
            hook_lock.unlock();
            return;
        }

        if (allocation_sites.has(return_address)) {
            allocation_sites.put(return_address, site);
            #ifdef GUARD_ACCESSES
            // allocation.protect(PROT_NONE);
            #endif
        } else if (!allocation_sites.full()) {
            allocation_sites.put(return_address, site);
            #ifdef GUARD_ACCESSES
            // allocation.protect(PROT_NONE);
            #endif
        } else {
            stack_debugf("Unable to add allocation site\n");
        }

        // Go through interval tests and update them
        for (size_t i=0; i<tests.size(); i++) {
            if (!tests[i]->has_quit()) {
                stack_debugf("Running on_alloc for test %\n", tests[i]->name());
                allocation.log();
                tests[i]->on_alloc(allocation);
            }
        }


        stack_debugf("Releasing lock\n");
        hook_lock.unlock();
        schedule();

        stack_debugf("Leaving IntervalTestSuite::update\n");
    }

    bool contains(void *ptr) {
        return allocation_sites.reduce<bool>([&](auto _, AllocationSite const &site, bool acc) {
            if (acc || site.allocations.has(ptr)) {
                return true;
            }
            return acc;
        }, false);
        /*
        // for (size_t i=0; i<allocation_sites.max_size(); i++) {
        //     if (allocation_sites.nth_entry(i).occupied) {
        //         AllocationSite site = allocation_sites.nth_entry(i).value;
        //         if (site.allocations.has(ptr)) {
        //             return true;
        //         }
        //     }
        // }
        */
        return false;
    }

    void invalidate(void *ptr) {
        // std::lock_guard<std::mutex> lock(hook_lock);
        stack_debugf("IntervalTestSuite::invalidate\n");
        stack_debugf("Invalidating %X\n", ptr);
        
        setup_protection_handler();

        // allocation_sites.map([&](uintptr_t key, AllocationSite &site) {
        //     if (site.allocations.has(ptr)) {
        //         site.allocations.remove(ptr);
        //     }
        // });
        schedule();
        hook_lock.lock();
        assert(allocation_sites.reduce<bool>([&](auto key, AllocationSite &site, bool found) {
            if (!found && site.allocations.has(ptr)) {
                for (size_t i=0; i<tests.size(); i++) {
                    if (!tests[i]->has_quit()) {
                        tests[i]->on_free(site.allocations.get(ptr));
                    }
                }

                site.allocations.remove(ptr);
                allocation_sites.put(site.return_address, site);
                return true;
            }
            return found;
        }, false));
        hook_lock.unlock();
        /*
        for (size_t i=0; i<allocation_sites.max_size(); i++) {
            if (allocation_sites.nth_entry(i).occupied) {
                AllocationSite site = allocation_sites.nth_entry(i).value;
                if (site.allocations.has(ptr)) {
                    stack_debugf("Waiting for hook lock to invalidate\n");
                    // hook_lock.lock();
                    stack_debugf("Invalidating %X\n", ptr);
                    site.allocations.remove(ptr);
                    allocation_sites.put(site.return_address, site);
                    stack_debugf("Done invalidating %X\n", ptr);
                    // hook_lock.unlock();
                }
            }
        }
        */

        stack_debugf("Leaving IntervalTestSuite::invalidate\n");
    }

    void access(void *address, bool is_write) {
        static std::mutex access_lock;
        // std::lock_guard<std::mutex> lock(access_lock);
        hook_lock.lock();
        become_working_thread();

        stack_debugf("Got access at %X\n", address);
        // Go through allocations until we find the one that contains the address
        bool found = allocation_sites.reduce<bool>([&](auto key, AllocationSite &site, bool found) {
            stack_debugf("Checking site at %X\n", site.return_address);
            if (found) {
                return true;
            }
            return site.allocations.reduce<bool>([&](auto key, Allocation &alloc, bool found) {
                if (found) {
                    return true;
                }
                if (alloc.contains(address)) {
                    stack_debugf("Found allocation at %X with size %d\n", (uintptr_t)alloc.ptr, alloc.size);
                    for (size_t i=0; i<tests.size(); i++) {
                        if (!tests[i]->has_quit()) {
                            tests[i]->on_access(alloc, is_write);
                            if (is_write) {
                                tests[i]->on_write(alloc);
                                #ifdef GUARD_ACCESSES
                                alloc.protect(PROT_WRITE);
                                #endif
                            } else {
                                tests[i]->on_read(alloc);
                                #ifdef GUARD_ACCESSES
                                alloc.protect(PROT_READ);
                                #endif
                            }
                            #ifdef SOFT_GUARD_ACCESSES
                            alloc.protect(PROT_READ | PROT_WRITE);
                            #endif
                        }
                    }
                    return true;
                }
                return false;
            }, false);
        }, false);
        if (!found) {
            stack_errorf("Access at %X not found\n", address);
            exit(1);
        }

        no_longer_working_thread();
        hook_lock.unlock();
    }

    bool is_done() const {
        // Are all the tests done?
        for (size_t i=0; i<tests.size(); i++) {
            if (!tests[i]->has_quit()) {
                return false;
            }
        }
        return true;
    }

    void finish() {
        static bool is_finished = false;
        if (is_finished) {
            return;
        }
        stack_debugf("IntervalTestSuite::finish\n");
        interval();
        cleanup();
        is_finished = true;
    }

    static IntervalTestSuite *get_instance();

    ~IntervalTestSuite() {
        finish();
    }

private:
    void heart_beat() {
        if (second_timer.has_elapsed(1000)) {
            stack_infof("Heart beat (1 second has elapsed)\n");
            if (timer.elapsed_milliseconds() >= config.period_milliseconds) {
                stack_infof("Interval timer is at %dms, test is ready!\n", timer.elapsed_milliseconds());
            } else {
                stack_infof("Interval timer is at %dms, test is not ready\n", timer.elapsed_milliseconds());
            }

            second_timer.reset();
        }
    }

    /// @brief Attempt to schedule a test interval, but bail if the timer hasn't elapsed enough time
    void schedule() {
        heart_beat();
        stack_debugf("IntervalTestSuite::schedule\n");
        // static std::mutex schedule_lock;
        // std::lock_guard<std::mutex> lock(schedule_lock);
        // schedule_lock.lock();
        if (!hook_lock.try_lock()) {
            stack_debugf("Unable to lock hook\n");
            return;
        } else {
            stack_debugf("Locked hook\n");
        }

        if (timer.elapsed_milliseconds() > config.period_milliseconds) {
            interval();
        } else {
            stack_debugf("Only %fms have elapsed, not yet at %fms interval\n", timer.elapsed_milliseconds(), config.period_milliseconds);
        }
        // schedule_lock.unlock();
        hook_lock.unlock();
        stack_debugf("IntervalTestSuite::schedule\n");
    }

    /// @brief Run the interval for all the tests
    void interval() {
        // Time the interval
        Timer interval_timer;
        interval_timer.start();

        setup_protection_handler();
        stack_logf("IntervalTestSuite::interval\n");
        // static std::mutex interval_lock;
        // std::lock_guard<std::mutex> lock(interval_lock);
        // std::lock_guard<std::mutex> lock2(hook_lock);
        is_in_interval = true;
        become_working_thread();
        timer.reset();

        // Go through all the access addresses, provide the test with the addresses
        // and clear the addresses
        StackVec<void*, MAX_TRACKED_ACCESSES> writes = get_written_addresses();
        writes.map([&](void *address) {
            if (!contains(address)) {
                return;
            }
            Allocation &alloc = alloc_from_addr(address);

            for (size_t i=0; i<tests.size(); i++) {
                if (!tests[i]->has_quit()) {
                    tests[i]->on_access(alloc, true);
                    tests[i]->on_write(alloc);
                }
            }
        });
        
        StackVec<void*, MAX_TRACKED_ACCESSES> reads = get_read_addresses();
        reads.map([&](void *address) {
            if (!contains(address)) {
                return;
            }
            Allocation &alloc = alloc_from_addr(address);

            for (size_t i=0; i<tests.size(); i++) {
                if (!tests[i]->has_quit()) {
                    tests[i]->on_access(alloc, false);
                    tests[i]->on_read(alloc);
                }
            }
        });

        clear_accesses();


        stack_infof("Running interval\n");
        // interval_lock.lock();
        for (size_t i=0; i<tests.size(); i++) {
            if (!tests[i]->has_quit()) {
                stack_infof("Running interval for test %d: %\n", i, tests[i]->name());
                tests[i]->interval(allocation_sites);
                stack_infof("Test %d (%) has finished\n", i, tests[i]->name());
            } else {
                stack_warnf("Test %d has quit\n", i);
            }
        }
        // If the test is done, clear the soft dirty bits
        if (config.clear_soft_dirty_bits) {
            stack_warnf("Clearing soft dirty bits\n");
            perform_clear_soft_dirty_bits();
        }
        
        timer.reset();
        // Tick the age of all the allocations
        allocation_sites.map([](uintptr_t key, AllocationSite &site) {
            site.tick_age();
        });

        #ifdef GUARD_ACCESSES
        #ifndef SOFT_GUARD_ACCESSES
        // protect_allocations();
        #endif
        #endif

        no_longer_working_thread();
        is_in_interval = false;
        stack_infof("IntervalTestSuite::interval -- Interval done\n");
        // End time
        uint64_t time_taken = interval_timer.elapsed_milliseconds();
        stack_infof("Finished interval in %d milliseconds\n", time_taken);
    }

    Allocation &alloc_from_addr(void *addr) {
        Allocation *alloc = NULL;
        allocation_sites.map([&](uintptr_t key, AllocationSite &site) {
            if (alloc != NULL) return;
            site.allocations.map([&](void *key, Allocation &allocation) {
                if (alloc != NULL) return;
                if (allocation.contains(addr)) {
                    alloc = &allocation;
                }
            });
        });
        if (alloc == NULL) {
            stack_errorf("Allocation not found for address %p\n", addr);
        }
        return *alloc;
    }

    void cleanup() {
        static bool is_cleaned = false;
        if (is_cleaned) {
            return;
        }
        stack_debugf("IntervalTestSuite::cleanup\n");
        for (size_t i=0; i<tests.size(); i++) {
            tests[i]->cleanup();
        }
        is_cleaned = true;
    }

    StackVec<IntervalTest*, 10> tests;
    Timer timer, second_timer;
    IntervalTestConfig config;

    StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> allocation_sites;

    // This is used to protect the main thread while we're compressing
    std::mutex hook_lock;

    bool is_in_interval = false;

    bool is_setup = false;
};

IntervalTestConfig config = INTERVAL_CONFIG;
static IntervalTestSuite interval_test_suite(config);

IntervalTestSuite *IntervalTestSuite::get_instance() {
    stack_debugf("Getting instance of IntervalTestSuite\n");
    return &interval_test_suite;
}

// This is the handler for SIGSEGV. It's called when we try to access
// a protected page. This will put the thread to sleep until we finish
// compressing the page. This thread will then be woken up when we
// unprotect the page.
static void protection_handler(int sig, siginfo_t *si, void *ucontext)
{
    stack_infof("PROTECTION HANDLER: Entering segfault handler\n");
    ucontext_t *context = (ucontext_t *)ucontext;
    static std::mutex protection_lock;
    std::lock_guard<std::mutex> lock(protection_lock);

    if (si->si_addr == NULL) {
        stack_errorf("Caught NULL pointer access: segfault at NULL\n");
        exit(1);
    } else {
        stack_debugf("Caught segfault at %p\n", si->si_addr);
    }
    long page_size = sysconf(_SC_PAGESIZE);
    void* aligned_address = (void*)((uint64_t)si->si_addr & ~(page_size - 1));
    u64 error_code = context->uc_mcontext.gregs[REG_ERR];
    bool is_write = error_code & 0x2;


    if (si->si_code == SEGV_ACCERR) {
        // printf("Invalid permissions for %s.\n", (si->si_code & 2) ? "write" : "read");
        stack_infof("Invalid permissions for %s.\n",is_write? "write" : "read");
    }
    
    if (is_working_thread()) {
        stack_warnf("Working thread, giving back access: 0x%X\n", (uint64_t)si->si_addr);
        // if (mprotect(aligned_address, getpagesize(), PROT_NONE) == -1) {
        //     perror("mprotect");
        //     exit(1);
        // }
        if (is_write) {
            if (mprotect(aligned_address, getpagesize(), PROT_WRITE | PROT_READ) == -1) {
                perror("mprotect");
                exit(1);
            }
        } else {
            if (mprotect(aligned_address, getpagesize(), PROT_WRITE | PROT_READ) == -1) {
                perror("mprotect");
                exit(1);
            }
        }
    } else {
        // If the access is a write, add it to the write page faults
        stack_debugf("Error code: %x\n", error_code);
        if (IS_PROTECTED) {
            stack_warnf("PROTECTION HANDLER: Caught access of temporarily protected memory 0x%X\n", (void*)si->si_addr);
            while (IS_PROTECTED) {}
        }
        
        // if (is_write) {
        //     stack_infof("Caught write page fault at 0x%X\n", (void*)si->si_addr);
        //     mprotect(aligned_address, getpagesize(), PROT_WRITE);
        //     // write_page_faults.insert(aligned_address);
        // } else {
        //     stack_infof("Caught read page fault at 0x%X\n", (void*)si->si_addr);
        //     mprotect(aligned_address, getpagesize(), PROT_READ);
        //     // read_page_faults.insert(aligned_address);
        // }
        // if (is_write) {
        //     stack_infof("Giving back write access to 0x%X\n", (void*)si->si_addr);
        //     if (mprotect(aligned_address, getpagesize(), PROT_WRITE | PROT_NONE) == -1) {
        //         perror("mprotect");
        //         exit(1);
        //     }
        // } else {
        //     stack_infof("Giving back read access to 0x%X\n", (void*)si->si_addr);
        //     if (mprotect(aligned_address, getpagesize(), PROT_READ | PROT_NONE) == -1) {
        //         perror("mprotect");
        //         exit(1);
        //     }
        // }

        // if (mprotect(aligned_address, getpagesize(), PROT_WRITE | PROT_READ | PROT_EXEC) == -1) {
        //     perror("mprotect");
        //     exit(1);
        // }
        // IntervalTestSuite::get_instance()->access(si->si_addr, is_write);

        if (is_write) {
            mark_address_written(si->si_addr);
        } else {
            mark_address_read(si->si_addr);
        }

        #ifdef GUARD_ACCESSES
        #ifndef SOFT_GUARD_ACCESSES
        if (is_write) {
            stack_debugf("Giving back write access to 0x%X\n", (void*)si->si_addr);
            if (mprotect(aligned_address, getpagesize(), PROT_WRITE | PROT_EXEC) == -1) {
                perror("mprotect");
                exit(1);
            }
        } else {
            stack_debugf("Giving back read access to 0x%X\n", (void*)si->si_addr);
            if (mprotect(aligned_address, getpagesize(), PROT_READ | PROT_EXEC) == -1) {
                perror("mprotect");
                exit(1);
            }
        }
        #else
        stack_infof("Giving back read and write access to 0x%X\n", (void*)si->si_addr);
        if (mprotect(aligned_address, getpagesize(), PROT_WRITE | PROT_READ | PROT_EXEC) == -1) {
            perror("mprotect");
            exit(1);
        }
        #endif
        #endif
        // if (is_write) {
        //     mprotect(aligned_address, getpagesize(), PROT_WRITE);
        // } else {
        //     mprotect(aligned_address, getpagesize(), PROT_READ);
        // }
    }

    stack_infof("PROTECTION HANDLER: Leaving segfault handler\n");
}