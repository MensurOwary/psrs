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

int* regularSamples; 		// each thread will save the local regular samples here
int* pivots; 			// the selected pivots will be stored here
int* partitions; 		// partition indices per thread (separated by the outer array index), and there is T+1 indices. 1 (start), 1 (end), and T-1 (middle)
int* mergedPartitionLength; 	// each thread will save its length here

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


// should be called as
// PROGRAM size thread_count
int main(){
	SIZE 	=  512000000;
	INPUT 			= generateArrayOfSize(SIZE);
	struct timeval* start = getTime();
	qsort(INPUT, SIZE, ISIZE, cmpfunc);
	long int diff = endTiming(start);
	printf("Took : %ld\n", diff);
	isSorted();
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
