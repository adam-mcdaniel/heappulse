#include <stdio.h>
#include <stdlib.h>

int main() {
	volatile int *memory = malloc(4096 * sizeof(char));
	printf("%c\n", memory[100000]);
}
