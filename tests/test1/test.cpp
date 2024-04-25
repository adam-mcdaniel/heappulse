#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <cstring>

int main() {
    printf("Hello World!\n");
    srand(10);
    for (int i=0; i<20; i++) {
        if (i % 2 == 0)
            printf("Tick\n");
        else
            printf("Tock\n");
        fflush(stdout);
        const int SIZE = 1000;
        char *ptr = (char*)malloc(SIZE * (rand() % 10 + 5));
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        // for (int j=0; j<SIZE/10; j++) {
        //     if (rand() % 2 == 0) {
        //         ptr[j * 10] = rand() % 256;
        //     }
        // }
        strcpy(ptr, "Hello World!");

        free(ptr);
    }
    printf("Hello World!\n");
    return 0;
}