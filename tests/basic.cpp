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
        volatile void *ptr = alloc(PAGE_SIZE);
        printf("ptr: %p\n", ptr);
        for (int j=0; j<PAGE_SIZE; j+=8) {
            *(u64 *)(ptr + j) = 0xdeadbeef;
            printf("ptr: %p, value: %llu\n", ptr + j, *(u64 *)(ptr + j));
        }
        *(u64 *)ptr = 100;
        printf("ptr: %p, value: %llu\n", ptr, *(u64 *)ptr);
        free((void *)ptr);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    printf("Done\n");
}