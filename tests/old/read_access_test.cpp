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
    srand(SEED);


    // Allocate some data, write to it once, wait, and then read from it continually for the next minute.
    u64 size = 32 * PAGE_SIZE;
    char* data = (char*)alloc(size);

    for (u64 i = 0; i < size; i++) {
        data[i] = rand() % 256;
    }


    // Wait for a bit
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Read from the data
    for (int j=0; j<10; j++) {
        u64 sum = 0;
        for (u64 i = 0; i < size; i++) {
            printf("%c", isprint(data[i]) ? data[i] : '.');
            sum += (u64)data[i];
        }
        printf("Writing to %p\n", data);
        data[0] = 'a';
        std::this_thread::sleep_for(std::chrono::seconds(5));
        volatile void *some_new_data = alloc(1000);
        free((void*)some_new_data);
        printf("Sum: %u\n", sum);
        std::this_thread::sleep_for(std::chrono::seconds(5));
        some_new_data = alloc(1000);
        *(char*)some_new_data = 'a';
        free((void*)some_new_data);
    }
}