#include <mpi.h>

int cmpfunc (const void * a, const void * b) { return ( *(int *) a - *(int*)b );}

void printArray(int* a, int size) {
	for (int i = 0; i < size; i++) printf("%d ", a[i]);
	printf("\n");
}

int findInitialMinPos(int * indices, int size) {
	for (int i = 0; i < size; i+=2)
		if (indices[i] != indices[i+1]) return i;
	return -1;
}

void isSorted(int* arr, int size) {
	for (int i = 0; i < size - 1; i++) {
		if (arr[i] > arr[i+1]) {
			printf("Not sorted: %d > %d\n", arr[i], arr[i+1]);
			return;	
		}
	}
	printf("Sorted\n");
}

static inline int bytes(int size) {
	return sizeof(int) * size;
}

static inline int* intAlloc(int size) {
	return malloc(bytes(size));
}

int* generateArrayDefault(int size) {
	srandom(15);
	int* r = intAlloc(size);
	for (int i = 0; i < size; i++) r[i] = (int) random();
	return r;
}
