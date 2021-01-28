#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define SIZE 36
#define T 3

int cmpfunc (const void * a, const void * b) {
	return ( *(int *) a - *(int*)b );
}

struct thread_data {
	int start;
	int end;
};

pthread_barrier_t barrier;

int* INPUT;

void isSorted() {
	int flag = 1;
	for (int i = 0; i < SIZE - 1; i++) {
		if (INPUT[i] > INPUT[i+1]) {
			flag = 0;
			break;	
		}
	}
	if (flag) {
		printf("Sorted\n");
	} else {
		printf("Not sorted\n");
	}
}

int* generateArrayOfSize(int size) {
	srandom(15);
	int* randoms = malloc(sizeof(int) * size);
	for (int i = 0; i < size; i++) {
		randoms[i] = (int) random();
	}
	return randoms;
}
void* phase1(void *arg) {
	struct thread_data* data = (struct thread_data*) arg;
	printf("the first element : %d\n", INPUT[data->start]);
	qsort((INPUT + data->start), (data->end - data->start), sizeof(int), cmpfunc);
	pthread_barrier_wait(&barrier);
}

void printArray() {
	for (int i = 0; i < SIZE; i++) {
		printf("%d ", INPUT[i]);
	}
	printf("\n");
}

int main(){
	INPUT = generateArrayOfSize(SIZE);
	printArray();
	int perThread = SIZE / T;
	
	pthread_barrier_init(&barrier, NULL, T + 1);

	for (int i = 0; i < T; i++) {
		pthread_t tid;
		struct thread_data* data = malloc(sizeof(struct thread_data));
		data->start = i * perThread;
		data->end = data->start + perThread;
		pthread_create(&tid, NULL, phase1, (void*) data); 
	}

	pthread_barrier_wait(&barrier);
	printArray();
	printf("Main thread finished\n");
	return 0;
}
