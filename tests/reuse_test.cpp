#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <sys/mman.h>
#include <ctype.h>

#define PAGE_SIZE 4096
#define SEED 0x12345678
#define u64 unsigned long long

// #define alloc(x) calloc(x, 1)
// #define alloc(x) malloc(x)
void *mmap_malloc(size_t size) {
    // Use mmap to allocate memory
    void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    // Check if mmap was successful
    if (ptr == MAP_FAILED) {
        return NULL; // Allocation failed
    }

    return ptr;
}
#define alloc(x) mmap_malloc(x)


int main() {
    srand(SEED);
    u64 number_of_pages = 16;

    void **ptrs = (void**)malloc(20000 * sizeof(void*));
    int ptrs_index = 0;
    for (int interval=0; interval<40; interval++) {
        u64 number_of_bytes = number_of_pages * PAGE_SIZE;

        char *ptr = (char*)alloc(number_of_bytes);
        ptrs[ptrs_index++] = ptr;
        
        printf("Interval %d: Reusing the first %d pointers\n", interval, (ptrs_index + 1) / 2);
        for (int cur_ptr=0; cur_ptr<(ptrs_index + 1)/2; cur_ptr++) {
            printf("  %08llX\n", (u64)ptrs[cur_ptr]);
        }
        for (int cur_ptr=0; cur_ptr<(ptrs_index + 1)/2; cur_ptr++) {
            ptr = (char*)ptrs[cur_ptr];
            
            // volatile char *ptr = (char*)mmap(NULL, number_of_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            // Use half of the allocated memory
            printf("Writing to %08llX (size=%lld)\n", (u64)ptr, number_of_bytes/4);
            for (int j=0; j<number_of_bytes / 4; j++) {
                ptr[j] = rand() % 256;
            }
            
            for (int j=0; j<number_of_bytes / 256; j++) {
                // printf("%c", ptr[j]);
                if (isalnum(ptr[j])) {
                    printf("%X ", ptr[j]);
                } else if (ptr[j] == '\0') {
                    printf(". ");
                } else {
                    printf("? ");
                }
            }

            printf("Allocated %llu pages\n", number_of_pages);
            printf("Wrote to %llu bytes\n", number_of_bytes/4);
            fflush(stdout);
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
        // volatile int *x = (int*)alloc(1000);
        // *x = 10;
        // printf("x = %d\n", *x);
        // free((void*)x);
        // free(ptr);
    }
    printf("Hello World!\n");
    return 0;
}