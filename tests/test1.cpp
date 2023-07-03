#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include <thread>

int main() {
    for (int i=0; i<250; i++) {
        void *ptr = malloc(10);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    printf("Hello World!\n");
    return 0;
}