#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>

#define MASTER if (id == 0) 
#define ISIZE sizeof(int)

int SIZE; 
int T;
int W; 
int RO; 

int* INPUT;

struct thread_data {
	int id;
	int start;
	int end;
};

int cmpfunc(const void* a, const void* b);
void isSorted();
struct timeval* getTime();
long int endTiming(struct timeval* start);
int* generateArrayOfSize(int size);

pthread_barrier_t barrier;

void* demo(void* a) {
	
	int i = *((int*) a);
	if (i == 2) {
		sleep(3);
	}
	
	printf("T%d executes...and waits at 1. barrier\n", i);
	pthread_barrier_wait(&barrier);
	
	if (i == 2) {
		sleep(3);
	}
	printf("T%d executes...and waits at 2. barrier\n", i);
	pthread_barrier_wait(&barrier);
	
	printf("All threads have finished\n");	

	return NULL;
}

// should be called as
// PROGRAM size thread_count
int main(){
	pthread_barrier_init(&barrier, NULL, 4);
	for (int i = 0; i < 3; i++) {
		pthread_t p1;
		int* arg = malloc(sizeof(int));
		*arg = i;
		pthread_create(&p1, NULL, demo, arg);
	}
	pthread_barrier_wait(&barrier);
	return 0;
}

// checks if the INPUT array is sorted
// used for debugging reasons
void isSorted() {
	int flag = 1;
	for (int i = 0; i < SIZE - 1; i++) {
		if (INPUT[i] > INPUT[i+1]) {
			flag = 0;
			printf("Not sorted: %d > %d\n", INPUT[i], INPUT[i+1]);
			break;	
		}
	}
	if (flag) {
		printf("Sorted\n");
	} else {
		printf("Not sorted\n");
	}
}

struct timeval* getTime() {
	struct timeval* t = malloc(sizeof(struct timeval));
	gettimeofday(t, NULL);
	return t;
}

long int endTiming(struct timeval* start) {
	struct timeval end; gettimeofday(&end, NULL);
	long int diff = (long int) ((end.tv_sec * 1000000 + end.tv_usec) - (start->tv_sec * 1000000 + start->tv_usec));
	free(start);
	return diff;
}

// used for generating random arrays of the given size
int* generateArrayOfSize(int size) {
	srandom(15);
	int* randoms = malloc(ISIZE * size);
	for (int i = 0; i < size; i++) {
		randoms[i] = (int) random();
	}
	return randoms;
}

// prints the values of the given array
void printArray(int* array, int size) {
	for (int i = 0; i < size; i++) {
		printf("%d ", array[i]);
	}
	printf("\n");
}


int cmpfunc (const void * a, const void * b) {
	return ( *(int *) a - *(int*)b );
}
