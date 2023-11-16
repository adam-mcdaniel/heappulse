#define BKMALLOC_HOOK
#include "bkmalloc.h"
#include <zlib.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include <vector>
#include <signal.h>
#include <unistd.h>
#include <csetjmp>
#include <thread>
#include <sys/mman.h>
#include <condition_variable>
#include <dlfcn.h>
#include <execinfo.h>

#define OUTPUT_CSV "bucket_stats.csv"
#define ALLOCATION_SITE_OUTPUT_CSV "allocation_site_stats.csv"

const int INTERVAL_MS = 10000;  // Interval in seconds to track memory usage (in milliseconds)
// const int INTERVAL_MS = 1000;  // Interval in seconds to track memory usage (in milliseconds)
const int MAX_TRACKED_ALLOCS = 1000000;
const int BACKTRACE_DEPTH = 32;
const int MAX_TRACKED_SITES = 1000;
const u64 BUCKET_SIZES[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 65536, 131072, 262144, 524288, 1048576, 2097152, 4194304, 8388608, 1073741824};
const int NUM_BUCKETS = sizeof(BUCKET_SIZES) / sizeof(BUCKET_SIZES[0]);
static bool IS_PROTECTED = false;

/* Obtain a backtrace and print it to stdout. */
// void print_trace() {
//     void *array[10];
//     char **strings;
//     int size, i;

//     size = backtrace(array, 10);
//     strings = backtrace_symbols(array, size);
//     if (strings != NULL) {
//         // printf("Obtained %d stack frames.\n", size);
//         std::cout << "Start Trace:" << std::endl;
//         for (i = 0; i < size; i++)
//             std::cout << i + 1 << ". " << strings[i] << std::endl;
//             // printf("%s\n", strings[i]);
//         free(strings);
//         std::cout << "End Trace" << std::endl;
//     } else {
//         std::cout << "Error" << std::endl;
//     }
// }

const char *get_path_to_file_from_code_address(void *address) {
    Dl_info dl_info;
    if (dladdr(address, &dl_info)) {
        // std::cerr << "Executable Name: " << dl_info.dli_fname << std::endl;
        // std::cerr << "Symbol Name: " << (dl_info.dli_sname != NULL? dl_info.dli_sname : "Null") << std::endl;
        return dl_info.dli_fname;
    } else {
        std::cerr << "Error: Unable to get the executable name.\n";
        exit(1);
    }
}

// std::mutex get_addr_mutex;
// void get_line_from_code_address(void *address, char *buf) {
//     volatile std::lock_guard<std::mutex> lock(get_addr_mutex);
//     Dl_info dl_info;
//     dladdr(address, &dl_info);
//     // Call `addr2line` on the command line using the address
//     // and the executable name from `get_path_to_file_from_code_address`
//     char command[1024];
//     command[0] = 0;
//     // sprintf(command, "addr2line -e %s %lld", dl_info.dli_fname, address);
//     // // std::string command = "addr2line -e " + executable_name + " " + std::to_string((long)address);
//     // // std::string result = "";
//     // FILE *fp = popen(command, "r");
//     // if (fp == NULL) {
//     //     std::cerr << "Failed to run command: " << command << std::endl;
//     //     exit(1);
//     // }
//     // fgets(buf, sizeof(buf), fp);
// }

struct Timer {
    std::chrono::steady_clock::time_point start_time;

    Timer() : start_time(std::chrono::steady_clock::now()) { }

    ~Timer() {
        printf("Elapsed time: %lu microseconds\n", elapsed_microseconds());
    }

    void start() {
        this->reset();
    }
    
    void reset() {
        this->start_time = std::chrono::steady_clock::now();
    }

    bool has_elapsed(u64 milliseconds) {
        return this->elapsed_milliseconds() >= milliseconds;
    }

    u64 elapsed_seconds() {
        return this->elapsed_milliseconds() / 1000;
    }

    u64 elapsed_milliseconds() {
        return this->elapsed_microseconds() / 1000;
    }

    u64 elapsed_microseconds() {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - this->start_time).count();
        return duration;
    }
};

template <typename KeyType, typename ValueType, std::size_t Size>
struct StackMap {
    struct Entry {
        KeyType key;
        ValueType value;
        bool occupied;

        Entry() : occupied(false) {}
        Entry(const KeyType& key) : key(key), occupied(true) {}
        Entry(const KeyType& key, const ValueType& value) : key(key), value(value), occupied(true) {}
    };

    // std::vector<Entry> hashtable;
    std::array<Entry, Size> hashtable;

    int entries = 0;

    std::size_t hash(const KeyType& key) const {
        return std::hash<KeyType>{}(key) % Size;
    }

    StackMap() : hashtable() {
        clear();
    }

    void put(const KeyType& key, const ValueType& value) {
        std::size_t index = hash(key);
        while (hashtable[index].occupied) {
            if (hashtable[index].key == key) {
                hashtable[index].value = value;  // Update value if key already exists
                return;
            }
            index = (index + 1) % Size;  // Linear probing for collision resolution
        }

        hashtable[index].key = key;
        hashtable[index].value = value;
        hashtable[index].occupied = true;
        entries++;
    }

    ValueType get(const KeyType& key) const {
        std::size_t index = hash(key);
        while (hashtable[index].occupied) {
            if (hashtable[index].key == key) {
                return hashtable[index].value;
            }
            index = (index + 1) % Size;  // Linear probing for collision resolution
        }
        // Key not found
        throw std::out_of_range("Key not found");
    }

    void remove(const KeyType& key) {
        std::size_t index = hash(key);
        while (hashtable[index].occupied) {
            if (hashtable[index].key == key) {
                hashtable[index].occupied = false;
                entries--;
                return;
            }
            index = (index + 1) % Size;  // Linear probing for collision resolution
        }
        // Key not found
        throw std::out_of_range("Key not found");
    }

    bool has(const KeyType& key) const {
        std::size_t index = hash(key);
        while (hashtable[index].occupied) {
            if (hashtable[index].key == key) {
                return true;
            }
            index = (index + 1) % Size;  // Linear probing for collision resolution
        }
        return false;
    }

    void clear() {
        for (u64 i=0; i<Size; i++) {
            hashtable[i].occupied = false;
        }
        entries = 0;
    }

    int size() const {
        return Size;
    }

    int num_entries() const {
        return entries;
    }

    bool empty() const {
        return entries == 0;
    }

    bool full() const {
        return entries == Size;
    }

    void map(void (*func)(KeyType&, ValueType&)) {
        int j = 0;
        for (u64 i=0; i<Size; i++) {
            if (hashtable[i].occupied) {
                func(hashtable[i].key, hashtable[i].value);
                if (++j == entries) {
                    break;
                }
            }
        }
    }

    void print() const {
        for (int i=0; i<Size; i++) {
            if (hashtable[i].occupied) {
                std::cout << hashtable[i].key << ": " << hashtable[i].value << std::endl;
            }
        }
    }

    void print_stats() const {
        int num_collisions = 0;
        int max_collisions = 0;
        for (int i=0; i<Size; i++) {
            if (hashtable[i].occupied) {
                int collisions = 0;
                int index = i;
                while (hashtable[index].occupied) {
                    collisions++;
                    index = (index + 1) % Size;
                }
                num_collisions += collisions;
                if (collisions > max_collisions) {
                    max_collisions = collisions;
                }
            }
        }
        std::cout << "Number of entries: " << entries << std::endl;
        std::cout << "Number of collisions: " << num_collisions << std::endl;
        std::cout << "Max collisions for any given key: " << max_collisions << std::endl;
    }
};



class Backtrace {
public:
    Backtrace() {}

    static Backtrace capture() {
        bool protection = IS_PROTECTED;
        IS_PROTECTED = true;
        Backtrace bt;
        bt.backtrace_size = backtrace(bt.backtrace_addrs, BACKTRACE_DEPTH);
        IS_PROTECTED = protection;
        return bt;
    }

    // Get the allocation site from the backtrace
    void *get_allocation_site() {
        // Get the first symbol that doesn't have `libbkmalloc` or `hook` in it.
        // This is the allocation site.
        bool protection = IS_PROTECTED;
        IS_PROTECTED = true;
        for (int i=0; i<backtrace_size; i++) {
            char **strings = backtrace_symbols(backtrace_addrs + i, 1), *symbol = strings[0];

            if (i == backtrace_size - 1) {
                from_hook = strstr(symbol, "libstdc") != NULL || strstr(symbol, "libc") != NULL || strstr(symbol, "ld") != NULL || strstr(symbol, "libbkmalloc") != NULL || strstr(symbol, "hook") != NULL;
            }

            if (strstr(symbol, "libstdc") == NULL && strstr(symbol, "libbkmalloc") == NULL && strstr(symbol, "hook") == NULL && strstr(symbol, "libc") == NULL && strstr(symbol, "ld") == NULL) {
                free(strings);
                allocation_site = backtrace_addrs[i];
                IS_PROTECTED = protection;
                return allocation_site;
            }
            free(strings);
        }
        IS_PROTECTED = protection;
        return NULL;
    }

    bool is_from_hook() {
        if (from_hook == -1) {
            // If the backtrace starts at `libc` or `ld`, then it's from the hook.
            bool protection = IS_PROTECTED;
            IS_PROTECTED = true;
            char **strings = backtrace_symbols(backtrace_addrs + backtrace_size - 1, 1), *symbol = strings[0];
            from_hook = strstr(symbol, "libstdc") != NULL || strstr(symbol, "libc") != NULL || strstr(symbol, "ld") != NULL || strstr(symbol, "libbkmalloc") != NULL || strstr(symbol, "hook") != NULL;
            free(strings);
            IS_PROTECTED = protection;

            return from_hook;
        } else {
            return from_hook;
        }
    }

    friend std::ostream& operator<<(std::ostream& os, Backtrace& bt) {
        char **strings;

        bool protection = IS_PROTECTED;
        IS_PROTECTED = true;
        strings = backtrace_symbols(bt.backtrace_addrs, bt.backtrace_size);

        bt.get_allocation_site();
        if (bt.is_from_hook()) {
            os << "Error: Allocation site is from hook" << std::endl;
            for (int i = 0; i < bt.backtrace_size; i++)
                os << i + 1 << ". " << strings[i] << std::endl;
            os << "End Trace" << std::endl;
        } else {
            if (strings != NULL) {
                os << "Start Trace @ 0x" << std::hex << (long long int)bt.allocation_site << std::dec << std::endl;
                for (int i = 0; i < bt.backtrace_size; i++)
                    os << i + 1 << ". " << strings[i] << std::endl;
                os << "End Trace" << std::endl;
            } else {
                os << "Error" << std::endl;
            }
        }

        free(strings);
        IS_PROTECTED = protection;
        return os;
    }

private:
    int from_hook = -1;
    void *allocation_site = NULL;
    void *backtrace_addrs[BACKTRACE_DEPTH];
    int backtrace_size = 0;
};


struct CompressionEntry {
    void *allocation_address;
    u64 n_bytes;
    uLong compressed_size;
    void *return_address = NULL;
    // void *backtrace_addrs[BACKTRACE_DEPTH];
    // int backtrace_size = 0;
    Backtrace backtrace;

    CompressionEntry() : allocation_address(NULL), n_bytes(0), compressed_size(0) {}
    CompressionEntry(void *allocation_address, u64 n_bytes, uLong compressed_size) : allocation_address(allocation_address), n_bytes(n_bytes), compressed_size(compressed_size) {}
    CompressionEntry(void *allocation_address, u64 n_bytes, uLong compressed_size, void *return_address) : allocation_address(allocation_address), n_bytes(n_bytes), compressed_size(compressed_size), return_address(return_address) {}

    void capture_backtrace() {
        backtrace = Backtrace::capture();
    }

    friend std::ostream& operator<<(std::ostream& os, const CompressionEntry& entry) {
        os << "addr: " << entry.allocation_address << ", n_bytes: " << entry.n_bytes << ", compressed_size: " << entry.compressed_size << ", return_address: " << entry.return_address << std::endl;
        return os;
    }
};

struct BucketEntry {
    double n_entries;
    double total_compressed_sizes;
    double total_uncompressed_sizes;
    BucketEntry() : n_entries(0), total_compressed_sizes(0) {}
    BucketEntry(double n_entries, double total_compressed_sizes) : n_entries(n_entries), total_compressed_sizes(total_compressed_sizes) {}
    BucketEntry(double n_entries, double total_compressed_sizes, double total_uncompressed_sizes) : n_entries(n_entries), total_compressed_sizes(total_compressed_sizes), total_uncompressed_sizes(total_uncompressed_sizes) {}

    friend std::ostream& operator<<(std::ostream& os, const BucketEntry& entry) {
        return os << "n_entries: " << entry.n_entries << ", total_compressed_sizes: " << entry.total_compressed_sizes << " avg_compressed_size: " << (double)entry.total_compressed_sizes / entry.n_entries << " avg_compression_ratio: " << (double)entry.total_compressed_sizes / entry.total_uncompressed_sizes;
    }
};

struct BucketKey {
    u64 lower_bytes_bound;
    u64 upper_bytes_bound;

    BucketKey() : lower_bytes_bound(0), upper_bytes_bound(0) {}

    BucketKey(u64 n_bytes) {
        if (n_bytes == 0) {
            lower_bytes_bound = 0;
            upper_bytes_bound = 0;
        } else {
            u64 bucket_size_index = 0;

            while (BUCKET_SIZES[bucket_size_index] < n_bytes) {
                bucket_size_index++;
            }

            if (bucket_size_index == 0) {
                lower_bytes_bound = 0;
                upper_bytes_bound = BUCKET_SIZES[0];
            } else if (bucket_size_index < NUM_BUCKETS) {
                lower_bytes_bound = BUCKET_SIZES[bucket_size_index - 1];
                upper_bytes_bound = BUCKET_SIZES[bucket_size_index];
            } else {
                lower_bytes_bound = BUCKET_SIZES[NUM_BUCKETS-1];
                upper_bytes_bound = std::numeric_limits<u64>::max();
            }
        }
    }

    static BucketKey nth(int n) {
        return BucketKey(BUCKET_SIZES[n]);
    }

    bool is_in_bucket(u64 n_bytes) {
        return n_bytes >= lower_bytes_bound && n_bytes < upper_bytes_bound;
    }

    bool operator==(const BucketKey& other) const {
        return lower_bytes_bound == other.lower_bytes_bound && upper_bytes_bound == other.upper_bytes_bound;
    }

    bool operator!=(const BucketKey& other) const {
        return !(*this == other);
    }

    bool operator<(const BucketKey& other) const {
        return lower_bytes_bound < other.lower_bytes_bound;
    }

    bool operator>(const BucketKey& other) const {
        return lower_bytes_bound > other.lower_bytes_bound;
    }

    bool operator<=(const BucketKey& other) const {
        return lower_bytes_bound <= other.lower_bytes_bound;
    }

    bool operator>=(const BucketKey& other) const {
        return lower_bytes_bound >= other.lower_bytes_bound;
    }

    friend std::ostream& operator<<(std::ostream& os, const BucketKey& key) {
        os << "lower_bytes_bound: " << key.lower_bytes_bound << ", upper_bytes_bound: " << key.upper_bytes_bound;
        return os;
    }
};

struct AllocationSiteKey {
    void *return_address = NULL;
    const char *path_to_file = NULL;
    int line_number = -1;

    AllocationSiteKey(Backtrace bt) {
        return_address = bt.get_allocation_site();
        path_to_file = get_path_to_file_from_code_address(return_address);
        line_number = -1;
    }

    AllocationSiteKey() : return_address(NULL), path_to_file(NULL), line_number(-1) {}

    AllocationSiteKey(void *return_address) : return_address(return_address) {}
    AllocationSiteKey(void *return_address, const char *path_to_file) : return_address(return_address), path_to_file(path_to_file) {}
    AllocationSiteKey(void *return_address, const char *path_to_file, int line_number) : return_address(return_address), path_to_file(path_to_file), line_number(line_number) {}

    bool operator==(const AllocationSiteKey& other) const {
        return return_address == other.return_address && path_to_file == other.path_to_file && line_number == other.line_number;
    }
};

namespace std {
  template <> struct hash<BucketKey>
  {
    size_t operator()(const BucketKey &key) const
    {
        return hash<u64>()(key.lower_bytes_bound) ^ hash<u64>()(key.upper_bytes_bound);
    }
  };

template <> struct hash<AllocationSiteKey>
{
    size_t operator()(const AllocationSiteKey &key) const
    {
        return hash<void*>()(key.return_address) ^ hash<const char*>()(key.path_to_file) ^ hash<int>()(key.line_number);
    }
};
}

struct AllocationSiteEntry {
    // For each allocation site, put each allocation size into a bucket
    StackMap<BucketKey, BucketEntry, NUM_BUCKETS> buckets_map;

    void add_compression_entry(CompressionEntry entry) {
        // Put it into the proper bucket, based on the size of the allocation
        BucketKey bucket_key(entry.n_bytes);
        BucketEntry bucket_entry;
        if (buckets_map.has(bucket_key)) {
            // Update the bucket entry
            bucket_entry = buckets_map.get(bucket_key);
            bucket_entry.total_compressed_sizes += entry.compressed_size;
            bucket_entry.total_uncompressed_sizes += entry.n_bytes;
            bucket_entry.n_entries++;
        } else {
            // Create a new bucket entry
            bucket_entry.total_compressed_sizes = entry.compressed_size;
            bucket_entry.total_uncompressed_sizes = entry.n_bytes;
            bucket_entry.n_entries = 1;
        }
        // Insert the bucket entry into the map
        buckets_map.put(bucket_key, bucket_entry);
    }
};

static StackMap<void*, CompressionEntry, MAX_TRACKED_ALLOCS> stack_alloc_map;
std::mutex alloc_map_mutex;

static StackMap<BucketKey, BucketEntry, NUM_BUCKETS> buckets_map;
std::mutex buckets_map_mutex;

static StackMap<AllocationSiteKey, AllocationSiteEntry, MAX_TRACKED_SITES> sites_map;
std::mutex sites_mutex;


std::mutex protect_mutex;


static u64 WORKING_THREAD_ID = 0;

bool is_working_thread() {
    return WORKING_THREAD_ID == (u64)pthread_self() && IS_PROTECTED;
}

void become_working_thread() {
    WORKING_THREAD_ID = (u64)pthread_self();
    IS_PROTECTED = true;
}

void no_longer_working_thread() {
    WORKING_THREAD_ID = 0;
    IS_PROTECTED = false;
}

/*
void unprotect_allocation(void *addr) {
    long page_size = sysconf(_SC_PAGESIZE);
    unsigned long address = (unsigned long)addr;
    void* aligned_address = (void*)(address & ~(page_size - 1));
    if (mprotect(aligned_address, page_size, PROT_READ | PROT_WRITE | PROT_EXEC) == -1) {
        perror("mprotect");
        exit(1);
    } else {
        char buf[] = "successfully unprotected\n";
        write(STDOUT_FILENO, buf, strlen(buf));
    }
}
*/

std::jmp_buf jump_buffer;

void check_compression_stats();

// This is the handler for SIGSEGV. It's called when we try to access
// a protected page. This will put the thread to sleep until we finish
// compressing the page. This thread will then be woken up when we
// unprotect the page.
static void protection_handler(int sig, siginfo_t *si, void *unused)
{
    // std::cout << "Got SIGSEGV at address: 0x" << std::hex << si->si_addr << std::endl;
    char buf[1024];
    // sprintf(buf, "Got SIGSEGV at address: 0x%lx\n", (long) si->si_addr);
    // write(STDOUT_FILENO, buf, strlen(buf));

    // Is this thread the main?
    if (is_working_thread()) {
        // sprintf(buf, "[FAULT] Working thread, giving back access: 0x%lx\n", (long) si->si_addr);
        // write(STDOUT_FILENO, buf, strlen(buf));

        long page_size = sysconf(_SC_PAGESIZE);
        void* aligned_address = (void*)((unsigned long)si->si_addr & ~(page_size - 1));
        mprotect(aligned_address, getpagesize(), PROT_READ | PROT_WRITE | PROT_EXEC);
    } else {
        if (!IS_PROTECTED) {
            sprintf(buf, "Dereferenced address 0x%lx when unprotected, halting program\n", (long) si->si_addr);
            write(STDOUT_FILENO, buf, strlen(buf));
            usleep(250000);
        }
        sprintf(buf, "[FAULT] Not the working thread: 0x%lx\n", (long) si->si_addr);
        write(STDOUT_FILENO, buf, strlen(buf));
        while (IS_PROTECTED) {}

        sprintf(buf, "[FAULT] Got access: 0x%lx\n", (long) si->si_addr);
        write(STDOUT_FILENO, buf, strlen(buf));
    }

    // Put the thread to sleep until we finish compressing the page.
    // This thread will then be woken up when we unprotect the page.

    // check_compression_stats();
    // while (IS_PROTECTED) {
    //     write(STDOUT_FILENO, "Waiting...", 10);
    //     sleep(1);
    // }
    // sleep(1);

    // if (IS_PROTECTED) {
    //     // std::longjmp(jump_buffer, 1);
    //     // Align the address to the page boundary
    //     long page_size = sysconf(_SC_PAGESIZE);
    //     void* aligned_address = (void*)((unsigned long)si->si_addr & ~(page_size - 1));
    //     mprotect(aligned_address, getpagesize(), PROT_READ | PROT_WRITE | PROT_EXEC);
    //     sleep(1);
    // } else {
    //     sprintf(buf, "Not protected, exiting\n");
    //     write(STDOUT_FILENO, buf, strlen(buf));
    // }
}
/*
void protect_allocation(void *addr) {
    volatile std::lock_guard<std::mutex> guard(protect_mutex);

    // WORKING_THREAD_ID = (u64)pthread_self();
    // IS_PROTECTED = true;
    become_working_thread();

    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = protection_handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESTART;

    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    } else {
        std::cout << "sigaction successful" << std::endl;
    }

    long page_size = sysconf(_SC_PAGESIZE);
    unsigned long address = (unsigned long)addr;
    void* aligned_address = (void*)(address & ~(page_size - 1));
    if (mprotect(aligned_address, page_size, PROT_READ | PROT_EXEC) == -1) {
        perror("mprotect");
        exit(1);
    } else {
        std::cout << "mprotect successful " << std::endl;
    }

    std::cout << "Protected allocation" << std::endl;
}*/

// This sets the SEGV signal handler to be the protection handler.
// The protection handler will capture the segfault caused by mprotect,
// and keep the program blocked until the compression tests are done
void setup_protection_handler()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = protection_handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;

    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    } else {
        // std::cout << "sigaction successful" << std::endl;
    }
}

// Go through each of the allocations, and protect them with mprotect
// so that they're read-only while we're compressing them.
void protect_allocation_entries()
{
    // volatile std::lock_guard<std::mutex> lock1(alloc_map_mutex);
    volatile std::lock_guard<std::mutex> lock2(protect_mutex);
    become_working_thread();
    setup_protection_handler();

    long page_size = sysconf(_SC_PAGESIZE);

    std::cout << "Protecting allocations" << std::endl;
    for (int i=0; i<stack_alloc_map.size(); i++) {
        if (stack_alloc_map.hashtable[i].occupied) {
            // std::cout << "Protecting " << stack_alloc_map.hashtable[i].value << std::endl;
            // This protects the entire page from being written to.
            unsigned long address = (unsigned long)stack_alloc_map.hashtable[i].value.allocation_address;
            void* aligned_address = (void*)(address & ~(page_size - 1));
            if (mprotect(aligned_address, page_size, PROT_READ | PROT_EXEC) == -1) {
                perror("mprotect");
                exit(1);
            } else {
                // std::cout << "protected allocation entry" << std::endl;
            }

            /*
            // This protects only the allocation from being written to.
            if (mprotect(stack_alloc_map.hashtable[i].value.addr, stack_alloc_map.hashtable[i].value.n_bytes, PROT_READ | PROT_EXEC) == -1) {
                std::cerr << "Failed to protect " << stack_alloc_map.hashtable[i].value << std::endl;
                perror("mprotect");
                exit(1);
            } else {
                std::cout << "protected allocation entry" << std::endl;
            }
            */
        }
    }
    std::cout << "Done protecting allocations" << std::endl;
}

// Go through each of the allocations, and unprotect them with mprotect
// so that they're read-write again.
void unprotect_allocation_entries() {
    // volatile std::lock_guard<std::mutex> lock1(alloc_map_mutex);
    volatile std::lock_guard<std::mutex> lock2(protect_mutex);

    long page_size = sysconf(_SC_PAGESIZE);

    std::cout << "Unprotecting allocations" << std::endl;
    for (int i=0; i<stack_alloc_map.size(); i++) {
        if (stack_alloc_map.hashtable[i].occupied) {
            // std::cout << "Unprotecting " << stack_alloc_map.hashtable[i].value << std::endl;
            // This unprotects the entire page from being written to.
            unsigned long address = (unsigned long)stack_alloc_map.hashtable[i].value.allocation_address;
            void* aligned_address = (void*)(address & ~(page_size - 1));
            if (mprotect(aligned_address, page_size, PROT_READ | PROT_WRITE | PROT_EXEC) == -1) {
                perror("mprotect");
                exit(1);
            } else {
                // std::cout << "mprotect successful" << std::endl;
            }

            /*
            // This unprotects only the allocation from being written to.
            if (mprotect(stack_alloc_map.hashtable[i].value.addr, stack_alloc_map.hashtable[i].value.n_bytes, PROT_READ | PROT_WRITE | PROT_EXEC) == -1) {
                std::cerr << "Failed to unprotect " << stack_alloc_map.hashtable[i].value << std::endl;
                perror("mprotect");
                exit(1);
            } else {
                std::cout << "mprotect successful" << std::endl;
            }
            */
        }
    }
    std::cout << "Done unprotecting allocations" << std::endl;

    no_longer_working_thread();
}

void record_alloc(void *addr, CompressionEntry entry) {
    if (!alloc_map_mutex.try_lock()) {
        return;
    } else {
        alloc_map_mutex.unlock();
    }
    volatile std::lock_guard<std::mutex> lock(alloc_map_mutex);
    
    stack_alloc_map.put(addr, entry);
}

void record_free(void *addr) {
    if (!stack_alloc_map.has(addr)) {
        return;
    }
    volatile std::lock_guard<std::mutex> lock(alloc_map_mutex);
    try {
        mprotect(addr, getpagesize(), PROT_READ | PROT_WRITE | PROT_EXEC);
        stack_alloc_map.remove(addr);
    } catch (std::exception& e) {
        std::cout << "Failed to remove " << addr << " from stack_alloc_map" << std::endl;
    }
}

CompressionEntry get_compression_entry(void *addr) {
    volatile std::lock_guard<std::mutex> lock(alloc_map_mutex);
    // return alloc_map[addr];
    return stack_alloc_map.get(addr);
}

const int MAX_COMPRESSED_SIZE = BUCKET_SIZES[NUM_BUCKETS - 1];
static Bytef compressed_data[MAX_COMPRESSED_SIZE];

// Create a function to be mapped over the entries
void check_compression_entry(void *&addr, CompressionEntry& entry) {
    char *input_data = (char*)addr;
    const size_t input_size = entry.n_bytes;
    // char input_copy[entry.n_bytes];
    // memcpy(input_copy, input_data, input_size);
    // input_data = input_copy;
    uLongf compressed_size = compressBound(input_size);
    // Bytef compressed_data[compressed_size];

    // entry.print_backtrace();

    // Bytef compressed_data[1024];
    // Get page size
    long page_size = sysconf(_SC_PAGESIZE);
    // Get aligned address
    unsigned long dst_address = (unsigned long)compressed_data;
    void* aligned_dst_address = (void*)(dst_address & ~(page_size - 1));
    unsigned long src_address = (unsigned long)input_data;
    void* aligned_src_address = (void*)(src_address & ~(page_size - 1));

    uLongf estimated_compressed_size = compressed_size;
    int result = compress(compressed_data, &compressed_size, (const Bytef*)input_data, input_size);
    entry.compressed_size = compressed_size;

    // if (result != Z_OK) {
    //     // std::cout << "Compression failed" << std::endl;
    //     // std::cout << "Compression of " << input_size << " bytes failed" << std::endl;
    //     // std::cout << "   src_address: 0x" << std::hex << src_address << std::endl;
    //     // std::cout << "   dst_address: 0x" << std::hex << dst_address << std::endl;
    //     // std::cout << "   aligned_src_address: 0x" << std::hex << aligned_src_address << std::endl;
    //     // std::cout << "   aligned_dst_address: 0x" << std::hex << aligned_dst_address << std::endl;
    //     // std::cout << "   estimated_compressed_size: " << std::dec << estimated_compressed_size << std::endl;
    //     // std::cout << "   actual_compressed_size: " << std::dec << compressed_size << std::endl;
    // // } else {
    // //     std::cout << "Compression succeeded" << std::endl;
    // }
}


void put_into_buckets() {
    volatile std::lock_guard<std::mutex> lock1(alloc_map_mutex);
    volatile std::lock_guard<std::mutex> lock2(buckets_map_mutex);

    int j = 0;
    // Iterate over all entries in the map and get their sizes and put their compression stats into buckets
    for (int i=0; i<stack_alloc_map.size(); i++) {
        // Is the entry in the bucket map?
        if (stack_alloc_map.hashtable[i].occupied) {
            if (stack_alloc_map.hashtable[i].value.backtrace.is_from_hook()) {
                continue;
            }
            if (stack_alloc_map.num_entries() / 20 > 0 && i % (stack_alloc_map.num_entries() / 20) == 0) {
                std::cout << "Putting entry into buckets:" << std::endl;
                std::cout << stack_alloc_map.hashtable[i].value.backtrace << std::endl;
            }
            // Find what bucket the entry belongs to
            uLong compressed_size = stack_alloc_map.hashtable[i].value.compressed_size,
                uncompressed_size = stack_alloc_map.hashtable[i].value.n_bytes;
            
            BucketKey bucket_key(uncompressed_size);
            BucketEntry bucket_entry;
            if (buckets_map.has(bucket_key)) {
                // Update the bucket entry
                bucket_entry = buckets_map.get(bucket_key);
                bucket_entry.total_compressed_sizes += compressed_size;
                bucket_entry.total_uncompressed_sizes += uncompressed_size;
                bucket_entry.n_entries++;
                // std::cout << "Updating bucket entry: " << bucket_entry << std::endl;
            } else {
                // Create a new bucket entry
                bucket_entry.total_compressed_sizes = compressed_size;
                bucket_entry.total_uncompressed_sizes = uncompressed_size;
                bucket_entry.n_entries = 1;
                // std::cout << "Creating new bucket entry: " << bucket_key << " -> " << bucket_entry << std::endl;
            }
            // Insert the bucket entry into the map
            buckets_map.put(bucket_key, bucket_entry);
            if (++j == stack_alloc_map.entries) {
                break;
            }
        }
    }
    // std::cout << "Updated buckets" << std::endl;
}

void track_allocation_sites() {
    volatile std::lock_guard<std::mutex> lock1(alloc_map_mutex);
    volatile std::lock_guard<std::mutex> lock2(sites_mutex);

    // Iterate over the allocation map entries and put them into the sites map according to their return address
    int j = 0;
    for (int i=0; i<stack_alloc_map.size(); i++) {
        if (stack_alloc_map.hashtable[i].occupied) {
            if (stack_alloc_map.hashtable[i].value.backtrace.is_from_hook()) {
                continue;
            }
            if (stack_alloc_map.num_entries() / 20 > 0 && i % (stack_alloc_map.num_entries() / 20) == 0) {
                std::cout << "Putting entry into site buckets:" << std::endl;
                std::cout << stack_alloc_map.hashtable[i].value.backtrace << std::endl;
            }
            // void *return_address = stack_alloc_map.hashtable[i].value.return_address;
            // const char *path_to_file = get_path_to_file_from_code_address(return_address);
            // int line_number = -1;
            // char line[1024];

            // get_line_from_code_address(return_address, line);
            // std::cout << "line: " << line << std::endl;
            // std::cout << "path_to_file: " << path_to_file << std::endl;
            // std::cout << "return_address: " << return_address << std::endl;
            
            // AllocationSiteKey site_key(return_address, path_to_file, line_number);
            AllocationSiteKey site_key(stack_alloc_map.hashtable[i].value.backtrace);

            AllocationSiteEntry site_entry;
            if (sites_map.has(site_key)) {
                // Update the site entry
                site_entry = sites_map.get(site_key);
            }

            stack_alloc_map.hashtable[i].value.backtrace.get_allocation_site();

            // Create a new site entry
            site_entry.add_compression_entry(stack_alloc_map.hashtable[i].value);

            // if (i % (stack_alloc_map.size() / 20) == 0) {
            //     std::cout << stack_alloc_map.hashtable[i].value << std::endl;
            // }
            // Insert the site entry into the map
            sites_map.put(site_key, site_entry);
            if (++j == stack_alloc_map.entries) {
                break;
            }
        }
    }
}

void check_compression_stats() {
    // std::cout << "Checking compression stats" << std::endl;
    {
        volatile std::lock_guard<std::mutex> lock(alloc_map_mutex);
        protect_allocation_entries();
        // Iterate over all entries in the map
        stack_alloc_map.map(check_compression_entry);
        
        put_into_buckets();
        track_allocation_sites();
        std::cout << "Done with pass" << std::endl;

        unprotect_allocation_entries();
    }
}

bool file_exists(const char *name) {
    std::ifstream file(name);
    return file.good();
}

struct Hooks {
    Timer timer;
    std::ofstream csv_file;
    int n_reports = 0;
    std::mutex report_mutex, compression_stats_mutex;
    Hooks() : timer() {
        setup_protection_handler();
    }

    void compression_test() {
        if (IS_PROTECTED || !compression_stats_mutex.try_lock()) {
            return;
        }
        setup_protection_handler();
        if (timer.has_elapsed(INTERVAL_MS)) {
            // std::thread t1(check_compression_stats);
            // Wait for t1 to finish
            // t1.join();
            // Thread with pthreads
            // pthread_t t1;
            // pthread_create(&t1, NULL, (void* (*)(void*))check_compression_stats, NULL);
            // pthread_join(t1, NULL);
 
            // Old
            timer.reset();
            check_compression_stats();
            timer.reset();
            report();
            timer.reset();

            /*
            // Ancient
            // for (auto it=alloc_map.begin(); it!=alloc_map.end(); it++) {
            //     CompressionEntry *entry = &it->second;
            //     void *addr = entry->addr;
            //     u64 n_bytes = entry->n_bytes;

            //     if (n_bytes <= 4096) {
            //         const char *input_data = (const char*)addr;
            //         const size_t input_size = n_bytes;
            //         entry->compressed_size = compressBound(input_size);  // Get an upper bound for the compressed size
            //         // Bytef* compressed_data = (Bytef*)malloc(compressed_size);
            //         Bytef compressed_data[4096];

            //         int result = compress2(compressed_data, &entry->compressed_size, (const Bytef*)input_data, input_size, Z_BEST_COMPRESSION);
            //         if (result == Z_OK) {
            //             printf("Compression successful!\n");
            //             printf("Original size: %lu bytes\n", input_size);
            //             printf("Compressed size: %lu bytes\n", entry->compressed_size);
            //         } else {
            //             printf("Compression failed. Error code: %d\n", result);
            //         }
            //     }
            // }
            */
        }
        compression_stats_mutex.unlock();
    }

    void post_alloc(bk_Heap *heap, u64 n_bytes, u64 alignment, int zero_mem, void *allocation_address) {
        // if (IS_PROTECTED) {
        //     return;
        // }

        if (IS_PROTECTED) {
            return;
        }

        CompressionEntry entry;
        entry.compressed_size = 0;
        entry.allocation_address = allocation_address;
        entry.n_bytes = n_bytes;
        entry.capture_backtrace();
        record_alloc(allocation_address, entry);
        compression_test();

        // bk_Block *block;
        // u32       idx;
        // block = BK_ADDR_PARENT_BLOCK(addr);
        // idx   = block->meta.size_class_idx;
        // if (idx == BK_BIG_ALLOC_SIZE_CLASS_IDX) { idx = BK_NR_SIZE_CLASSES; }
        // hist[idx] += 1;
        // if (alloc_entry_idx < sizeof(alloc_arr) / sizeof(alloc_arr[0]))
        //     alloc_arr[alloc_entry_idx++] = { addr, n_bytes, 0 };
    }

    void pre_free(bk_Heap *heap, void *addr) {
        // std::cout << "Freeing " << addr << std::endl;
        
        // record_free(addr);
        // compression_test();

        record_free(addr);
        if (IS_PROTECTED) {
            return;
        }
        compression_test();

        // bk_Block *block;
        // u32       idx;
        // block = BK_ADDR_PARENT_BLOCK(addr);
        // idx   = block->meta.size_class_idx;
        // if (idx == BK_BIG_ALLOC_SIZE_CLASS_IDX) { idx = BK_NR_SIZE_CLASSES; }
        // hist[idx] -= 1;
    }

    void report() {
        volatile std::lock_guard<std::mutex> lock1(report_mutex);
        volatile std::lock_guard<std::mutex> lock2(alloc_map_mutex);
        volatile std::lock_guard<std::mutex> lock3(sites_mutex);
        volatile std::lock_guard<std::mutex> lock4(buckets_map_mutex);

        bool protection = IS_PROTECTED;
        IS_PROTECTED = true;

        n_reports++;

        if (n_reports == 1) {
            create_stats_csv();
        }

        double total_compressed_heap_size = 0, total_uncompressed_heap_size = 0;
        for (int i=0; i<NUM_BUCKETS; i++) {
            BucketKey key = BucketKey::nth(i);
            if (!buckets_map.has(key)) {
                continue;
            }
            BucketEntry entry = buckets_map.get(key);
            total_compressed_heap_size += entry.total_compressed_sizes;
            total_uncompressed_heap_size += entry.total_uncompressed_sizes;
        }

        std::cout << "Bucket stats:" << std::endl;
        for (int i=0; i<NUM_BUCKETS; i++) {
            std::cout << "Bucket " << i << " (" << BUCKET_SIZES[i] << " bytes)" << std::endl;
            BucketKey key = BucketKey::nth(i);
            if (!buckets_map.has(key)) {
                std::cout << "  No entries" << std::endl;
                continue;
            }
            BucketEntry entry = buckets_map.get(key);
            std::cout << "  Total compressed size: " << entry.total_compressed_sizes << std::endl;
            std::cout << "  Total uncompressed size: " << entry.total_uncompressed_sizes << std::endl;
            std::cout << "  Number of entries: " << entry.n_entries << std::endl;
            std::cout << "  Compression ratio (lower is better): " << (double)entry.total_compressed_sizes / (double)entry.total_uncompressed_sizes << std::endl;
            std::cout << "  Total portion of heap (uncompressed): " << (double)entry.total_uncompressed_sizes / total_uncompressed_heap_size << std::endl;
            std::cout << "  Total portion of heap (compressed): " << (double)entry.total_compressed_sizes / total_compressed_heap_size << std::endl;

            // csv_file << n_reports << "," << key.lower_bytes_bound << "-" << key.upper_bytes_bound << "," << entry.total_compressed_sizes << "," << entry.total_uncompressed_sizes << "," << entry.n_entries << "," << (double)entry.total_compressed_sizes / (double)entry.total_uncompressed_sizes << std::endl;

            csv_file << n_reports << "," // Interval #
                     << key.lower_bytes_bound << "-" << key.upper_bytes_bound << "," // Bucket Size
                     << entry.n_entries << "," // Number of Allocations
                     << (double)entry.total_uncompressed_sizes / (double)entry.total_compressed_sizes << "," // Buckets's Compression Efficiency (uncompressed/compressed)
                     << (double)entry.total_uncompressed_sizes / total_uncompressed_heap_size << "," // Bucket's Uncompressed Portion of Heap
                     << (double)entry.total_compressed_sizes / total_compressed_heap_size << "," // Bucket's Compressed Portion of Heap
                     << entry.total_uncompressed_sizes << "," // Bucket's Uncompressed Size
                     << entry.total_compressed_sizes << "," // Bucket's Compressed Size
                     << total_uncompressed_heap_size << "," // Total Uncompressed Heap Size
                     << total_compressed_heap_size << "," // Total Compressed Heap Size
                     << std::endl;
        }


        std::ofstream all_site_stats_csv;
        all_site_stats_csv.open(ALLOCATION_SITE_OUTPUT_CSV, (n_reports == 1? std::ios::trunc : std::ios::app) | std::ios::out);

        if (n_reports == 1) {
            all_site_stats_csv << "Interval # (" << INTERVAL_MS << " ms), "
                << "Allocation Site, "
                << "Bucket Size, "
                << "Buckets's Compression Efficiency (uncompressed/compressed), "
                << "Buckets Uncompressed Portion of Heap, "
                << "Buckets Compressed Portion of Heap, "
                << "Site's Portion of Heap (uncompressed), "
                << "Site's Portion of Heap (compressed), "
                << "Number of Allocations, "
                << "Bucket's Uncompressed Portion of Allocation Site's Data, "
                << "Bucket's Compressed Portion of Allocation Site's Data, "
                << "Bucket's Uncompressed Size, "
                << "Bucket's Compressed Size, "
                << "Total Uncompressed Allocation Site Data Size, "
                << "Total Compressed Allocation Site Data Size, "
                << "Total Uncompressed Heap Size, "
                << "Total Compressed Heap Size, "
                << std::endl;
        }

        std::ofstream site_stats_csv;
        std::cout << "Site stats:" << std::endl;
        for (int i=0; i<MAX_TRACKED_SITES; i++) {
            if (!sites_map.hashtable[i].occupied) {
                continue;
            }
            AllocationSiteKey site_key = sites_map.hashtable[i].key;
            std::cout << "  Site " << std::dec << i << " (" << (site_key.path_to_file == NULL? "Null" : site_key.path_to_file) << ":" << site_key.line_number << ") at address 0x" << std::hex << (long long int)site_key.return_address << std::endl;
            std::stringstream ss;
            ss << "site_stats_" << (site_key.path_to_file == NULL? "" : site_key.path_to_file) << "_" << std::hex << (long long int)site_key.return_address << std::dec;
            std::string site_stats_csv_name = ss.str();

            while (site_stats_csv_name.find("/") != std::string::npos) {
                site_stats_csv_name.replace(site_stats_csv_name.find("/"), 1, "_");
            }
            while (site_stats_csv_name.find(".") != std::string::npos) {
                site_stats_csv_name.replace(site_stats_csv_name.find("."), 1, "_");
            }
            site_stats_csv_name += ".csv";

            if (file_exists(site_stats_csv_name.c_str())) {
                std::cout << "    File " << site_stats_csv_name << " already exists" << std::endl;
                // Open the file for appending
                site_stats_csv.open(site_stats_csv_name, std::ios::app | std::ios::out);
            } else {
                std::cout << "    File " << site_stats_csv_name << " does not exist" << std::endl;
                // Create the file
                site_stats_csv.open(site_stats_csv_name, std::ios::trunc | std::ios::out);
                // Write the header
                site_stats_csv << "Interval # (" << INTERVAL_MS << " ms), "
                    << "Bucket Size, "
                    << "Buckets's Compression Efficiency (uncompressed/compressed), "
                    << "Buckets Uncompressed Portion of Heap, "
                    << "Buckets Compressed Portion of Heap, "
                    << "Site's Portion of Heap (uncompressed), "
                    << "Site's Portion of Heap (compressed), "
                    << "Number of Allocations, "
                    << "Bucket's Uncompressed Portion of Allocation Site's Data, "
                    << "Bucket's Compressed Portion of Allocation Site's Data, "
                    << "Bucket's Uncompressed Size, "
                    << "Bucket's Compressed Size, "
                    << "Total Uncompressed Allocation Site Data Size, "
                    << "Total Compressed Allocation Site Data Size, "
                    << "Total Uncompressed Heap Size, "
                    << "Total Compressed Heap Size, "
                    << std::endl;
            }

            AllocationSiteEntry site_entry = sites_map.hashtable[i].value;
            auto buckets_map = site_entry.buckets_map;
            

            double total_compressed_site_size = 0, total_uncompressed_site_size = 0;
            for (int j=0; j<NUM_BUCKETS; j++) {
                BucketKey key = BucketKey::nth(j);
                if (!buckets_map.has(key)) {
                    continue;
                }
                BucketEntry entry = buckets_map.get(key);
                total_compressed_site_size += entry.total_compressed_sizes;
                total_uncompressed_site_size += entry.total_uncompressed_sizes;
            }

            // Iterate over the buckets in the site
            for (int j=0; j<NUM_BUCKETS; j++) {
                std::cout << "    Bucket " << std::dec << j << " (" << BUCKET_SIZES[j] << " bytes)" << std::endl;

                BucketKey key = BucketKey::nth(j);
                if (!buckets_map.has(key)) {
                    std::cout << "      No entries" << std::endl;
                    continue;
                }
                BucketEntry entry = buckets_map.get(key);
                std::cout << "      Total compressed size: " << entry.total_compressed_sizes << std::endl;
                std::cout << "      Total uncompressed size: " << entry.total_uncompressed_sizes << std::endl;
                std::cout << "      Number of entries: " << entry.n_entries << std::endl;
                std::cout << "      Compression ratio (lower is better): " << (double)entry.total_compressed_sizes / (double)entry.total_uncompressed_sizes << std::endl;
                std::cout << "      Total portion of allocation site's data (uncompressed): " << (double)entry.total_uncompressed_sizes / total_uncompressed_site_size << std::endl;
                std::cout << "      Total portion of allocation site's data (compressed): " << (double)entry.total_compressed_sizes / total_compressed_site_size << std::endl;

                site_stats_csv << n_reports << "," // Interval #
                    << key.lower_bytes_bound << "-" << key.upper_bytes_bound << "," // Bucket Size
                    << (double)entry.total_uncompressed_sizes / (double)entry.total_compressed_sizes << "," // Buckets's Compression Efficiency (uncompressed/compressed)
                    << (double)entry.total_uncompressed_sizes / total_uncompressed_heap_size << "," // Bucket's Uncompressed Portion of Heap
                    << (double)entry.total_compressed_sizes / total_compressed_heap_size << "," // Bucket's Compressed Portion of Heap
                    << total_uncompressed_site_size / total_uncompressed_heap_size << "," // Allocation Site's Portion of heap (uncompressed)
                    << total_compressed_site_size / total_compressed_heap_size << "," // Allocation Site's Portion of heap (compressed)
                    << entry.n_entries << "," // Number of Allocations
                    << (double)entry.total_uncompressed_sizes / total_uncompressed_site_size << "," // Bucket's Uncompressed Portion of allocation site data
                    << (double)entry.total_compressed_sizes / total_compressed_site_size << "," // Bucket's Compressed Portion of allocation site data
                    << entry.total_uncompressed_sizes << "," // Bucket's Uncompressed Size
                    << entry.total_compressed_sizes << "," // Bucket's Compressed Size
                    << total_uncompressed_site_size << "," // Total portion of allocation site's data (uncompressed)
                    << total_compressed_site_size << "," // Total portion of allocation site's data (compressed)
                    << total_uncompressed_heap_size << "," // Total Uncompressed Heap Size
                    << total_compressed_heap_size << "," // Total Compressed Heap Size
                    << std::endl;

                all_site_stats_csv << n_reports << "," // Interval #
                    << site_key.path_to_file << " at address" << site_key.return_address << "," // Site's Return Address
                    << key.lower_bytes_bound << "-" << key.upper_bytes_bound << "," // Bucket Size
                    << (double)entry.total_uncompressed_sizes / (double)entry.total_compressed_sizes << "," // Buckets's Compression Efficiency (uncompressed/compressed)
                    << (double)entry.total_uncompressed_sizes / total_uncompressed_heap_size << "," // Bucket's Uncompressed Portion of Heap
                    << (double)entry.total_compressed_sizes / total_compressed_heap_size << "," // Bucket's Compressed Portion of Heap
                    << total_uncompressed_site_size / total_uncompressed_heap_size << "," // Allocation Site's Portion of heap (uncompressed)
                    << total_compressed_site_size / total_compressed_heap_size << "," // Allocation Site's Portion of heap (compressed)
                    << entry.n_entries << "," // Number of Allocations
                    << (double)entry.total_uncompressed_sizes / total_uncompressed_site_size << "," // Bucket's Uncompressed Portion of Heap
                    << (double)entry.total_compressed_sizes / total_compressed_site_size << "," // Bucket's Compressed Portion of Heap
                    << entry.total_uncompressed_sizes << "," // Bucket's Uncompressed Size
                    << entry.total_compressed_sizes << "," // Bucket's Compressed Size
                    << total_uncompressed_site_size << "," // Total portion of allocation site's data (uncompressed)
                    << total_compressed_site_size << "," // Total portion of allocation site's data (compressed)
                    << total_uncompressed_heap_size << "," // Total Uncompressed Heap Size
                    << total_compressed_heap_size << "," // Total Compressed Heap Size
                    << std::endl;
            }
            site_stats_csv.close();
        }
        all_site_stats_csv.close();
        IS_PROTECTED = protection;
    }

    void create_stats_csv() {
        if (csv_file.is_open()) {
            csv_file.close();
        }
        
        csv_file.open("bucket_stats.csv", std::ios::trunc | std::ios::out);
        // csv_file << "Iteration (" << INTERVAL_MS << " millisecond intervals), Bucket Size, Total Compressed Size, Total Uncompressed Size, Number of Compressions, Compression Ratio" << std::endl;

        csv_file << "Interval # (" << INTERVAL_MS << " ms), "
            << "Bucket Size, "
            << "Number of Allocations, "
            << "Buckets's Compression Efficiency (uncompressed/compressed), "
            << "Bucket's Uncompressed Portion of Heap, "
            << "Bucket's Compressed Portion of Heap, "
            << "Bucket's Uncompressed Size, "
            << "Bucket's Compressed Size, "
            << "Total Uncompressed Heap Size, "
            << "Total Compressed Heap Size, "
            << std::endl;
    }

    ~Hooks() {
        // printf("%-10s %16s\n", "SIZE CLASS", "COUNT");
        // printf("---------------------------\n");
        // for (i = 0; i < BK_NR_SIZE_CLASSES + 1; i += 1) {
        //     if (i == BK_NR_SIZE_CLASSES) {
        //         printf("%-10s %16lu\n", "BIG", this->hist[i]);
        //     } else {
        //         printf("%-10lu %16lu\n", bk_size_class_idx_to_size(i), this->hist[i]);
        //     }
        // }
        // stack_alloc_map.print();
        // stack_alloc_map.print_stats();
        compression_test();
        report();
        csv_file.close();
    }
};

static Hooks hooks;

extern "C"
void bk_post_alloc_hook(bk_Heap *heap, u64 n_bytes, u64 alignment, int zero_mem, void *addr) {
    // setup_protection_handler();
    hooks.post_alloc(heap, n_bytes, alignment, zero_mem, addr);
}

extern "C"
void bk_pre_free_hook(bk_Heap *heap, void *addr) {
    // setup_protection_handler();
    hooks.pre_free(heap, addr);
}
