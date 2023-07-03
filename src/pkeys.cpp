#define BKMALLOC_HOOK
#include "bkmalloc.h"
#include <zlib.h>
#include <iostream>
#include <stdio.h>
#include <chrono>
#include <vector>
#include <signal.h>
#include <unistd.h>
#include <err.h>
#include <stdlib.h>
#include <asm/prctl.h>
#include <sys/prctl.h>
#include <sys/mman.h>

struct Timer {
    std::chrono::steady_clock::time_point start_time;

    Timer() : start_time(std::chrono::steady_clock::now()) { }

    ~Timer() {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - this->start_time).count();
        printf("Elapsed time: %lu microseconds\n", duration);
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

    StackMap() : hashtable() {}

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
        for (int i=0; i<Size; i++) {
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
        for (int i=0; i<Size; i++) {
            if (hashtable[i].occupied) {
                func(hashtable[i].key, hashtable[i].value);
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


struct CompressionEntry {
    void *addr;
    u64 n_bytes;
    uLong compressed_size;

    friend std::ostream& operator<<(std::ostream& os, const CompressionEntry& entry) {
        os << "addr: " << entry.addr << ", n_bytes: " << entry.n_bytes << ", compressed_size: " << entry.compressed_size;
        return os;
    }
};

struct BucketEntry {
    u64 n_entries;
    double total_compressed_sizes;
    BucketEntry() : n_entries(0), total_compressed_sizes(0) {}
    BucketEntry(u64 n_entries, double total_compressed_sizes) : n_entries(n_entries), total_compressed_sizes(total_compressed_sizes) {}

    friend std::ostream& operator<<(std::ostream& os, const BucketEntry& entry) {
        os << "n_entries: " << entry.n_entries << ", total_compressed_sizes: " << entry.total_compressed_sizes;
        return os;
    }
};

const int BUCKET_SIZES[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
const int NUM_BUCKETS = sizeof(BUCKET_SIZES) / sizeof(BUCKET_SIZES[0]);

struct BucketKey {
    u64 lower_bytes_bound;
    u64 upper_bytes_bound;

    BucketKey() : lower_bytes_bound(0), upper_bytes_bound(0) {}

    BucketKey(u64 n_bytes) {
        if (n_bytes == 0) {
            lower_bytes_bound = 0;
            upper_bytes_bound = 0;
        } else {
            int bucket_size_index = 0;
            while (BUCKET_SIZES[bucket_size_index] < n_bytes) {
                bucket_size_index++;
            }
            lower_bytes_bound = BUCKET_SIZES[bucket_size_index - 1];
            upper_bytes_bound = BUCKET_SIZES[bucket_size_index];
        }
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

namespace std {
  template <> struct hash<BucketKey>
  {
    size_t operator()(const BucketKey &key) const
    {
        return hash<u64>()(key.lower_bytes_bound) ^ hash<u64>()(key.upper_bytes_bound);
    }
  };
}

static StackMap<void*, CompressionEntry, 1000> stack_alloc_map;
std::mutex alloc_map_mutex;

static StackMap<BucketKey, BucketEntry, NUM_BUCKETS> buckets_map;
std::mutex buckets_map_mutex;


static int PKEY = 0;

// This is the handler for SIGSEGV. It's called when we try to access
// a protected page. This will put the thread to sleep until we finish
// compressing the page. This thread will then be woken up when we
// unprotect the page.
static void protection_handler(int sig, siginfo_t *si, void *unused)
{
    // std::cout << "Got SIGSEGV at address: 0x" << std::hex << si->si_addr << std::endl;
    char buf[1024];
    sprintf(buf, "Got SIGSEGV at address: 0x%lx\n", (long) si->si_addr);
    write(STDOUT_FILENO, buf, strlen(buf));
    // Put the thread to sleep until we finish compressing the page.
    // This thread will then be woken up when we unprotect the page.
    return;
}

// Go through each of the allocations, and protect them with mprotect
// so that they're read-only while we're compressing them.
void protect_allocation_entries()
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
        std::cout << "sigaction successful" << std::endl;
    }

    long page_size = sysconf(_SC_PAGESIZE);

    std::cout << "Protecting allocations" << std::endl;
    for (int i=0; i<sizeof(stack_alloc_map.hashtable) / sizeof(stack_alloc_map.hashtable[0]); i++) {
        if (stack_alloc_map.hashtable[i].occupied) {
            std::cout << "Protecting " << stack_alloc_map.hashtable[i].value << std::endl;
            unsigned long address = (unsigned long)stack_alloc_map.hashtable[i].value.addr;
            void* aligned_address = (void*)(address & ~(page_size - 1));
            if (pkey_mprotect(aligned_address, stack_alloc_map.hashtable[i].value.n_bytes, PROT_READ | PROT_EXEC, PKEY) == -1) {
                perror("pkey_mprotect");
                exit(1);
            } else {
                std::cout << "mprotect successful" << std::endl;
            }
        }
    }
    std::cout << "Done protecting allocations" << std::endl;
}

// Go through each of the allocations, and unprotect them with mprotect
// so that they're read-write again.
void unprotect_allocation_entries() {
    long page_size = sysconf(_SC_PAGESIZE);

    int status = pkey_set(PKEY, PROT_WRITE | PROT_READ);
    if (status == -1)
        err(EXIT_FAILURE, "pkey_free");

    std::cout << "Unprotecting allocations" << std::endl;
    for (int i=0; i<sizeof(stack_alloc_map.hashtable) / sizeof(stack_alloc_map.hashtable[0]); i++) {
        if (stack_alloc_map.hashtable[i].occupied) {
            std::cout << "Unprotecting " << stack_alloc_map.hashtable[i].value << std::endl;
            unsigned long address = (unsigned long)stack_alloc_map.hashtable[i].value.addr;
            void* aligned_address = (void*)(address & ~(page_size - 1));
            if (mprotect(aligned_address, stack_alloc_map.hashtable[i].value.n_bytes, PROT_READ | PROT_WRITE | PROT_EXEC) == -1) {
                perror("mprotect");
                exit(1);
            } else {
                std::cout << "mprotect successful" << std::endl;
            }
        }
    }
    std::cout << "Done unprotecting allocations" << std::endl;
}

void record_alloc(void *addr, CompressionEntry entry) {
    if (!alloc_map_mutex.try_lock()) {
        return;
    } else {
        alloc_map_mutex.unlock();
    }
    std::lock_guard<std::mutex> lock(alloc_map_mutex);
    // alloc_map[addr] = entry;
    stack_alloc_map.put(addr, entry);
}

void record_free(void *addr) {
    std::lock_guard<std::mutex> lock(alloc_map_mutex);
    // alloc_map.erase(addr);
    stack_alloc_map.remove(addr);
}

CompressionEntry get_compression_entry(void *addr) {
    std::lock_guard<std::mutex> lock(alloc_map_mutex);
    // return alloc_map[addr];
    return stack_alloc_map.get(addr);
}

// Create a function to be mapped over the entries
void check_compression_entry(void *&addr, CompressionEntry& entry) {
    const char *input_data = (const char*)addr;
    const size_t input_size = entry.n_bytes;
    Bytef* compressed_data = (Bytef*)malloc(entry.compressed_size);
    uLongf compressed_size = compressBound(input_size);
    int result = compress(compressed_data, &compressed_size, (const Bytef*)input_data, input_size);
    if (result != Z_OK) {
        std::cout << "Compression failed" << std::endl;
        return;
    }
    entry.compressed_size = compressed_size;
}



void put_into_buckets() {
    std::lock_guard<std::mutex> lock1(alloc_map_mutex);
    std::lock_guard<std::mutex> lock2(buckets_map_mutex);

    // Iterate over all entries in the map and get their sizes and put their compression stats into buckets
    for (int i=0; i<stack_alloc_map.size(); i++) {
        // Is the entry in the bucket map?
        if (stack_alloc_map.hashtable[i].occupied) {
            // Find what bucket the entry belongs to
            int compression_size = stack_alloc_map.hashtable[i].value.compressed_size;
            BucketKey bucket_key(compression_size);
            BucketEntry bucket_entry;
            if (buckets_map.has(bucket_key)) {
                // Update the bucket entry
                bucket_entry = buckets_map.get(bucket_key);
                bucket_entry.total_compressed_sizes += compression_size;
                bucket_entry.n_entries++;
                std::cout << "Updating bucket entry: " << bucket_entry << std::endl;
                // Insert the bucket entry into the map
                buckets_map.put(bucket_key, bucket_entry);
            } else {
                // Create a new bucket entry
                bucket_entry.total_compressed_sizes = compression_size;
                bucket_entry.n_entries = 1;
                std::cout << "Creating new bucket entry: " << bucket_key << " -> " << bucket_entry << std::endl;
                // Insert the bucket entry into the map
                buckets_map.put(bucket_key, bucket_entry);
            }
        }
    }

    const int BUCKET_SIZES[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
    const int NUM_BUCKETS = sizeof(BUCKET_SIZES) / sizeof(BUCKET_SIZES[0]);
}

void check_compression_stats() {
    std::cout << "Checking compression stats" << std::endl;
    {
        std::lock_guard<std::mutex> lock(alloc_map_mutex);
        protect_allocation_entries();
        // Iterate over all entries in the map
        stack_alloc_map.map(check_compression_entry);
        unprotect_allocation_entries();
    }
    put_into_buckets();
}

struct Hooks {
    Timer timer;

    u64 hist[BK_NR_SIZE_CLASSES + 1];

    Hooks() : hist(), timer() {
        if (prctl(PR_SET_PKEY_DISABLE, 0, 0, 0, 0) == -1) {
            perror("prctl");
            return 1;
        }
        /*
        * Allocate a protection key:
        */
        PKEY = pkey_alloc(0, 0);
        if (PKEY == -1)
            err(EXIT_FAILURE, "pkey_alloc");

        /*
        * Disable access to any memory with "pkey" set,
        * even though there is none right now.
        */
        int status = pkey_set(PKEY, PROT_WRITE | PROT_READ);
        if (status)
            err(EXIT_FAILURE, "pkey_set");
    }

    void compression_test() {
        if (timer.has_elapsed(500)) {
            timer.reset();
            check_compression_stats();
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
        }
    }

    void post_alloc(bk_Heap *heap, u64 n_bytes, u64 alignment, int zero_mem, void *addr) {
        record_alloc(addr, {addr, n_bytes, 0});
        compression_test();
        bk_Block *block;
        u32       idx;

        block = BK_ADDR_PARENT_BLOCK(addr);
        idx   = block->meta.size_class_idx;

        if (idx == BK_BIG_ALLOC_SIZE_CLASS_IDX) { idx = BK_NR_SIZE_CLASSES; }
        hist[idx] += 1;

        // if (alloc_entry_idx < sizeof(alloc_arr) / sizeof(alloc_arr[0]))
        //     alloc_arr[alloc_entry_idx++] = { addr, n_bytes, 0 };
    }

    void post_free(bk_Heap *heap, void *addr) {
        record_free(addr);
        compression_test();
        bk_Block *block;
        u32       idx;

        block = BK_ADDR_PARENT_BLOCK(addr);
        idx   = block->meta.size_class_idx;

        if (idx == BK_BIG_ALLOC_SIZE_CLASS_IDX) { idx = BK_NR_SIZE_CLASSES; }
        hist[idx] -= 1;
    }

    ~Hooks() {
        u32 i;

        printf("%-10s %16s\n", "SIZE CLASS", "COUNT");
        printf("---------------------------\n");
        for (i = 0; i < BK_NR_SIZE_CLASSES + 1; i += 1) {
            if (i == BK_NR_SIZE_CLASSES) {
                printf("%-10s %16lu\n", "BIG", this->hist[i]);
            } else {
                printf("%-10lu %16lu\n", bk_size_class_idx_to_size(i), this->hist[i]);
            }
        }

        stack_alloc_map.print();
        stack_alloc_map.print_stats();
        buckets_map.print();
        buckets_map.print_stats();
    }
};

static Hooks hooks;

extern "C"
void bk_post_alloc_hook(bk_Heap *heap, u64 n_bytes, u64 alignment, int zero_mem, void *addr) {
    hooks.post_alloc(heap, n_bytes, alignment, zero_mem, addr);
}
