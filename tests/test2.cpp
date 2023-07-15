#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <thread>
#include <chrono>

#define MEMORY_SIZE 1024  // Size of the allocated memory

void* threadFunction(void* arg) {
    char* memory = (char*)malloc(MEMORY_SIZE);  // Allocate memory

    if (memory == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        pthread_exit(NULL);
    }

    // Loop until 10 seconds have elapsed
    auto start = std::chrono::high_resolution_clock::now();

    while (true) {
        if (std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::high_resolution_clock::now() - start)
                .count() > 10) {
            break;
        }
        // Write to the allocated memory
        // Here, we are simply writing 'A' to the memory repeatedly
        for (int i = 0; i < MEMORY_SIZE; i++) {
            int *x = (int*)malloc(10 * i);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            memory[i] = 'A';
            free(x);
        }
    }

    free(memory);
    pthread_exit(NULL);
}

int main() {
    pthread_t threads[2];

    for (int i = 0; i < 2; i++) {
        int result = pthread_create(&threads[i], NULL, threadFunction, NULL);
        if (result != 0) {
            fprintf(stderr, "Thread creation failed.\n");
            return 1;
        }
    }

    // Wait for the threads to finish (never executed)
    for (int i = 0; i < 2; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}
