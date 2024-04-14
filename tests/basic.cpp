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
#define alloc(x) malloc(x)
// #define alloc(x) mmap(NULL, x, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)
// #define free(x) munmap(x, 0)


int main() {
    for (int i=0; i<10; i++) {
        const int size = PAGE_SIZE * 3;
        volatile void *ptr = alloc(size);
        // printf("ptr: %p\n", ptr);
        for (int j=0; j<size; j+=8) {
            // for (int k=0; k<size && k<10; k++)
            //     printf("%02X", *((char *)(ptr + j) + k));
            // printf("\nRead from %p: %llX\n", ptr + j, *(u64 *)(ptr + j));
            // *(u64 *)(ptr + j) = 0xdeadbeef;
            // printf("Wrote %llX\n", *(u64 *)(ptr + j));

            // Alternate between reads and writes
            if (j % 16 == 0) {
                printf("Read from %p: %llX\n", ptr + j, *(u64 *)(ptr + j));
            } else {
                *(u64 *)(ptr + j) = 0xdeadbeef;
                printf("Wrote %llX\n", *(u64 *)(ptr + j));
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
        *(u64 *)ptr = 100;
        printf("Finished read/write loop for %p\n", ptr);
        free((void *)ptr);
    }

    printf("Done\n");
}