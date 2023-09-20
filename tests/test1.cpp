#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <thread>

int main() {
    printf("Hello World!\n");
    srand(10);
    for (int i=0; i<250; i++) {
        if (i % 2 == 0)
            printf("Tick\n");
        else
            printf("Tock\n");
        const int SIZE = 1000;
        char *ptr = (char*)malloc(SIZE);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        for (int j=0; j<SIZE/10; j++) {
            if (rand() % 2 == 0) {
                ptr[j * 10] = rand() % 256;
            }
        }

        if (rand() % 2 == 0) {
            free(ptr);
        }
    }
    printf("Hello World!\n");
    return 0;
}