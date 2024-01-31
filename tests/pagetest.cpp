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
#define alloc(x) mmap(NULL, x, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)
#define free(x) munmap(x, 0)

int main() {
    srand(SEED);
    u64 number_of_pages = 16;
    while (number_of_pages < (16 << 5)) {
        u64 number_of_bytes = number_of_pages * PAGE_SIZE;

        char *ptr = (char*)alloc(number_of_bytes);
        // volatile char *ptr = (char*)mmap(NULL, number_of_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        // Use half of the allocated memory
        printf("Writing to %08llX (size=%lld)\n", (u64)ptr, number_of_bytes);
        for (int j=0; j<number_of_bytes / 4; j++) {
            ptr[j] = rand() % 256;
        }
        
        for (int j=0; j<number_of_bytes / 2; j++) {
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
        printf("Wrote to %llu bytes\n", number_of_bytes/2);

        // Sleep for 10 seconds
        std::this_thread::sleep_for(std::chrono::seconds(15));
        volatile int *x = (int*)alloc(1000);
        *x = 10;
        printf("x = %d\n", *x);
        free((void*)x);
        // free(ptr);

        number_of_pages <<= 1;
    }
    printf("Hello World!\n");
    return 0;
}