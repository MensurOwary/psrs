#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>

#define SIZE 128000000 
#define T 4
#define W (SIZE/(T*T))
#define RO (T/2)
#define MASTER if (id == 0) 

int cmpfunc (const void * a, const void * b) {
	return ( *(int *) a - *(int*)b );
}

struct thread_data {
	int id;
	int start;
	int end;
};

pthread_barrier_t phase1Barrier;
pthread_barrier_t phase2Barrier;
pthread_barrier_t phase3Barrier;
pthread_barrier_t phase4Barrier;
pthread_barrier_t mergingPhaseBarrier;

int* INPUT;

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

// used for generating random arrays of the given size
int* generateArrayOfSize(int size) {
	srandom(15);
	int* randoms = malloc(sizeof(int) * size);
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

// returns the default array in the PSRS paper example
int* generateArrayDefault() {
	int arr[36] = {16, 2, 17, 24, 33, 28, 30, 1, 0, 27, 9, 25, 34, 23, 19, 18, 11, 7, 21, 13, 8, 35, 12, 29, 6, 3, 4, 14, 22, 15, 32, 10, 26, 31, 20, 5};
	
	int* randoms = malloc(sizeof(int) * 36);
	for (int i = 0; i < 36; i++) {
		randoms[i] = arr[i];
	}
	return randoms;
}

// given the exchangeIndices, it returns the value and index of the first valid values.
int* findInitialMin(int * exchangeIndices, int size) {
	for (int i = 0; i < size; i += 2) {
		if (exchangeIndices[i] != exchangeIndices[i+1]) {
			int* minAndPos = malloc(sizeof(int) * 2);
			minAndPos[0] = INPUT[exchangeIndices[i]];
			minAndPos[1] = i;
			return minAndPos;
		}
	}
	return NULL;
}

int regularSamples[T*T]; // each thread will save the local regular samples here
int pivots[T - 1]; // the selected pivots will be stored here
int partitions[T][T+1]; // partition indices per thread (separated by the outer array index), and there is T+1 indices. 1 (start), 1 (end), and T-1 (middle)
int mergedPartitionLength[T]; // each thread will save its length here

void* psrs(void *args) {
	struct thread_data* data = (struct thread_data*) args;
	int id = data->id;
	int start = data->start;
	int end = data->end;

	free(data); // data object is no longer needed
	/* Phase 1 */
	qsort((INPUT + start), (end - start), sizeof(int), cmpfunc);
	
	/* regular sampling */
	int ix = 0;
	for (int i = 0; i < T; i++) {
		regularSamples[id * T + ix] = INPUT[start + (i * W)];
		ix++;
	}	

	pthread_barrier_wait(&phase1Barrier);

	/* Phase 2 */
	MASTER { 
		qsort(regularSamples, T*T, sizeof(int), cmpfunc);
		ix = 0;
		for (int i = 1; i < T; i++) {
			int pos = T * i + RO - 1;
			pivots[ix++] = regularSamples[pos];
		}
	}
	pthread_barrier_wait(&phase2Barrier);

	/* Phase 3 */
	int pi = 0; // pivot counter
	int pc = 1; // partition counter
	partitions[id][0] = start;
	partitions[id][T] = end;
	for (int i = start; i < end && pi != T-1; i++) {
		if (pivots[pi] < INPUT[i]) {
			partitions[id][pc++] = i;
			pi++;
		}
	}
	
	pthread_barrier_wait(&phase3Barrier);

	/* Phase 4 */
	int exchangeIndices[T*2];
	int ei = 0;
	for (int i = 0; i < T; i++) {
		exchangeIndices[ei++] = partitions[i][id];
		exchangeIndices[ei++] = partitions[i][id+1];
	}
	// k way merge part
	// printArray(exchangeIndices, 2* T);
	// array size
	int mergedLength = 0;
	for (int i = 0; i < T * 2; i+=2) {
		mergedLength += exchangeIndices[i + 1] - exchangeIndices[i];
	}
	
	int* mergedValues = malloc(sizeof(int) * mergedLength);
	mergedPartitionLength[id] = mergedLength;
	int mi = 0; // mergedValues index
	while (mi < mergedLength) {
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
	pthread_barrier_wait(&phase4Barrier);
	
	int startPos = 0;
	int x = id - 1;
	while (x >= 0) {
		startPos += mergedPartitionLength[x--];
	}
	for (int i = startPos; i < startPos + mergedLength; i++) {
		INPUT[i] = mergedValues[i - startPos];
	}
	free(mergedValues);
	pthread_barrier_wait(&mergingPhaseBarrier);
	
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

int main(){
	printf("Size : %d\n", SIZE);
	INPUT = generateArrayOfSize(SIZE);
	// printArray(INPUT, SIZE);
	int perThread = SIZE / T;
	
	pthread_barrier_init(&phase1Barrier, NULL, T);
	pthread_barrier_init(&phase2Barrier, NULL, T);
	pthread_barrier_init(&phase3Barrier, NULL, T);
	pthread_barrier_init(&phase4Barrier, NULL, T);
	pthread_barrier_init(&mergingPhaseBarrier, NULL, T);
	
	pthread_t THREADS[T];

	struct timeval start;
	gettimeofday(&start, NULL);
	
	int i = 1;
	for (i = 1; i < T - 1; i++) {
		struct thread_data* data = getThreadData(i, perThread);
		pthread_create(&THREADS[i], NULL, psrs, (void *) data);
	}

	
	struct thread_data* data = getThreadData(i, perThread);
	data->end = SIZE;
	pthread_create(&THREADS[i], NULL, psrs, (void *) data);
	
	struct thread_data* data2 = getThreadData(0, perThread);
	psrs((void *) data2);
	
	printf("Threads finished\n");
	
	struct timeval end;
	gettimeofday(&end, NULL);

	long int diff = (long) ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));

	printf("It took %ld mics\n", diff);

	// printArray(INPUT, SIZE);
	isSorted();
	free(INPUT);
	return 0;
}
