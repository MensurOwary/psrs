#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>

#define MASTER if (id == 0) 
#define ISIZE sizeof(int)
#define BARRIER pthread_barrier_wait(&barrier)

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

pthread_barrier_t barrier;

int cmpfunc(const void* a, const void* b);
void isSorted();
struct timeval* getTime();
long int endTiming(struct timeval* start);
int* generateArrayOfSize(int size);
void printArray(int* array, int size);

/*
 * Phase 1
 * Does the local sorting of the array, and collects sample.
 */
void phase1(struct thread_data* data) {
	struct timeval* timeStart = getTime();
	
	int start = data->start;
	int end = data->end;
	int id = data->id;
	
	qsort((INPUT + start), (end - start), ISIZE, cmpfunc);
	
	/* regular sampling */
	int ix = 0;
	for (int i = 0; i < T; i++) {
		regularSamples[id * T + ix] = INPUT[start + (i * W)];
		ix++;
	}
	
	long int time = endTiming(timeStart);
	printf("Thread %d - Phase 1 took %ld ms, sorted %d items\n", id, time, (end - start));	
}

/*
 * Phase 2
 * Sequential part of the algorithm. Determines pivots based on the regular samples provided.
 */
void phase2(struct thread_data* data) {	
	int id = data->id;
	MASTER { 
		struct timeval* start = getTime();
		
		qsort(regularSamples, T*T, ISIZE, cmpfunc);
		int ix = 0;
		for (int i = 1; i < T; i++) {
			int pos = T * i + RO - 1;
			pivots[ix++] = regularSamples[pos];
		}
		
		long int time = endTiming(start);
		printf("Thread %d - Phase 2 took %ld ms\n", id, time);
	}
	
}

/*
 * Phase 3
 * Local splitting of the data based on the pivots
 */
void phase3(struct thread_data* data) {
	struct timeval* timeStart = getTime();

	int start = data->start;
	int end = data->end;
	int id = data->id;

	int pi = 0; // pivot counter
	int pc = 1; // partition counter
	partitions[id*(T+1)+0] = start;
	partitions[id*(T+1)+T] = end;
	for (int i = start; i < end && pi != T-1; i++) {
		if (pivots[pi] < INPUT[i]) {
			partitions[id*(T+1) + pc] = i;
			pc++;
			pi++;
		}
	}
	
	long int time = endTiming(timeStart);
	printf("Thread %d - Phase 3 took %ld ms\n", id, time);
}

/* 
 * Given the exchangeIndices, it returns the value and index of the first valid value.
 * 
 * Explanation: exchangeIndices is an array of the form [s1, e1, s2, e2...] where 
 * si represents the start of a range, and ei represents the end of that range.
 * 
 * A valid value means any si where si != ei
 * It is used for merging k sorted arrays, and this method finds the first valid value, 
 * so that it can be used as an initial minimum (as opposed to using INT_MAX)
 * 
 * returns: an array of 2 elements [minumum value, its position] 
 */
int* findInitialMin(int * exchangeIndices, int size) {
	for (int i = 0; i < size; i += 2) {
		if (exchangeIndices[i] != exchangeIndices[i+1]) {
			int* minAndPos = malloc(ISIZE * 2);
			minAndPos[0] = INPUT[exchangeIndices[i]];
			minAndPos[1] = i;
			return minAndPos;
		}
	}
	return NULL;
}

/*
 * Merges the array with a given size into the original INPUT array
 */
void mergeIntoOriginalArray(int id, int* array, int arraySize) {
	struct timeval* mergeStart = getTime();

	// find the position that the thread needs to start from
	// in order to put values into the original array
	// start position is basically the summation of 
	// the lengths of the previous partitions
	int startPos = 0;
	int x = id - 1;
	while (x >= 0) {
		startPos += mergedPartitionLength[x--];
	}
	for (int i = startPos; i < startPos + arraySize; i++) {
		INPUT[i] = array[i - startPos];
	}
	long int time = endTiming(mergeStart);
	printf("Thread %d - Phase Merge took %ld ms\n", id, time);	
	free(array);
}

/*
 * Phase 4
 * 
 * Merges the received partitions from other threads, and performs a k way merge
 * It also saves the merged values into their appropriate place in the INPUT array.
 */
void phase4(struct thread_data* data) {
	struct timeval* start = getTime();

	int exchangeIndices[T*2];
	int id = data->id;

	int ei = 0; // exchange indices counter
	for (int i = 0; i < T; i++) {
		exchangeIndices[ei++] = partitions[i*(T+1) + id];
		exchangeIndices[ei++] = partitions[i*(T+1) + id + 1];
	}
	// k way merge - start
	// array size
	int totalMergeLength = 0;
	for (int i = 0; i < T * 2; i+=2) {
		totalMergeLength += exchangeIndices[i + 1] - exchangeIndices[i];
	}
	
	int* mergedValues = malloc(ISIZE * totalMergeLength);
	mergedPartitionLength[id] = totalMergeLength;
	int mi = 0; // mergedValues index
	while (mi < totalMergeLength) {
		// find minimum among current items
		int* minAndPos = findInitialMin(exchangeIndices, T * 2);
		if (minAndPos == NULL) break;
		int min = minAndPos[0];
		int minPos = minAndPos[1];
		free(minAndPos);
		
		for (int i = 0; i < T * 2; i+=2) {
			if (exchangeIndices[i] != exchangeIndices[i+1]) {
				int ix = exchangeIndices[i];
				if (INPUT[ix] < min) {
					min = INPUT[ix];
					minPos = i;
				}
			}
		}
		mergedValues[mi++] = min;
		exchangeIndices[minPos]++;
	}
	// k way merge - end
	long int time = endTiming(start);
	BARRIER;
	
	printf("Thread %d - Phase 4 took %ld ms, merged %d keys\n", id, time, totalMergeLength);
	
	mergeIntoOriginalArray(id, mergedValues, totalMergeLength);
}

void* psrs(void *args) {
	struct thread_data* data = (struct thread_data*) args;
	int id = data->id; 

	/* Phase 1 */
	phase1(data);
	BARRIER;

	/* Phase 2 */
	phase2(data);
	BARRIER;

	/* Phase 3 */
	phase3(data);
	BARRIER;

	/* Phase 4 */
	phase4(data);
	BARRIER;

	free(data);	
	
	MASTER {
		return NULL;
	}
	pthread_exit(0);
}

struct thread_data* getThreadData(int id, int perThread) {
	struct thread_data* data = malloc(sizeof(struct thread_data));
	data->id = id;
	data->start = id * perThread;
	data->end = data->start + perThread;
	return data;
}


// should be called as
// PROGRAM size thread_count
int main(int argc, char *argv[]){
	if (argc != 3) {
		fprintf(stderr, "3 arguments required\n");
		exit(1);
	} 

	SIZE 	= atoi(argv[1]);
	T 	= atoi(argv[2]);
	W 	= (SIZE/(T*T));
	RO 	= T / 2;
	
	printf("Size : %d\n", SIZE);

	INPUT 			= generateArrayOfSize(SIZE);
	regularSamples 		= malloc(ISIZE *T*T); 
	pivots 			= malloc(ISIZE * (T - 1));
	mergedPartitionLength 	= malloc(ISIZE * T);
	partitions 		= malloc(ISIZE *  T * (T+1));

	int perThread = SIZE / T;
	
	pthread_barrier_init(&barrier, NULL, T);
	
	pthread_t* THREADS = malloc(sizeof(pthread_t) * T);

	struct timeval* start = getTime();
	
	int i = 1;
	for (i = 1; i < T - 1; i++) {
		struct thread_data* data = getThreadData(i, perThread);
		pthread_create(&THREADS[i], NULL, psrs, (void *) data);
	}
	// the last thread gets the remaining part of the array
	struct thread_data* data = getThreadData(i, perThread); data->end = SIZE;
	pthread_create(&THREADS[i], NULL, psrs, (void *) data);
	// master thread
	struct thread_data* dataMaster = getThreadData(0, perThread);
	psrs((void *) dataMaster);
	
	long int time = endTiming(start);

	printf("Took: %ld mics\n", time);
	
 	isSorted();	

	free(regularSamples);
	free(mergedPartitionLength);
	free(partitions);
	free(pivots);
	free(INPUT);
	free(THREADS);
	
	pthread_barrier_destroy(&barrier);
	return 0;
}

// checks if the INPUT array is sorted
// used for debugging and validation reasons
void isSorted() {
	for (int i = 0; i < SIZE - 1; i++) {
		if (INPUT[i] > INPUT[i+1]) {
			printf("Not sorted: %d > %d\n", INPUT[i], INPUT[i+1]);
			return;	
		}
	}
	printf("Sorted\n");
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

// Reference: https://stackoverflow.com/a/27284318/9985287
// integer compare function
int cmpfunc (const void * a, const void * b) { return ( *(int *) a - *(int*)b );}
