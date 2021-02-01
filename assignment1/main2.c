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

pthread_barrier_t phase1Barrier;
pthread_barrier_t phase2Barrier;
pthread_barrier_t phase3Barrier;
pthread_barrier_t phase4Barrier;
pthread_barrier_t mergingPhaseBarrier;

int cmpfunc (const void * a, const void * b) {
	return ( *(int *) a - *(int*)b );
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

// returns the default array in the PSRS paper example
int* generateArrayDefault() {
	int arr[36] = {16, 2, 17, 24, 33, 28, 30, 1, 0, 27, 9, 25, 34, 23, 19, 18, 11, 7, 21, 13, 8, 35, 12, 29, 6, 3, 4, 14, 22, 15, 32, 10, 26, 31, 20, 5};
	
	int* randoms = malloc(ISIZE * 36);
	for (int i = 0; i < 36; i++) {
		randoms[i] = arr[i];
	}
	return randoms;
}

// given the exchangeIndices, it returns the value and index of the first valid values.
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

int* regularSamples; 		// each thread will save the local regular samples here
int* pivots; 			// the selected pivots will be stored here
int* partitions; 		// partition indices per thread (separated by the outer array index), and there is T+1 indices. 1 (start), 1 (end), and T-1 (middle)
int* mergedPartitionLength; 	// each thread will save its length here

void* psrs(void *args) {
	struct thread_data* data = (struct thread_data*) args;
	int id = data->id;
	int start = data->start;
	int end = data->end;

	free(data); // data object is no longer needed
	/* Phase 1 */
	qsort((INPUT + start), (end - start), ISIZE, cmpfunc);
	
	/* regular sampling */
	int ix = 0;
	for (int i = 0; i < T; i++) {
		regularSamples[id * T + ix] = INPUT[start + (i * W)];
		ix++;
	}	

	pthread_barrier_wait(&phase1Barrier);

	/* Phase 2 */
	MASTER { 
		qsort(regularSamples, T*T, ISIZE, cmpfunc);
		ix = 0;
		for (int i = 1; i < T; i++) {
			int pos = T * i + RO - 1;
			pivots[ix++] = regularSamples[pos];
		}
		free(regularSamples);
	}
	pthread_barrier_wait(&phase2Barrier);

	/* Phase 3 */
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
	pthread_barrier_wait(&phase3Barrier);
	MASTER { free(pivots);}

	/* Phase 4 */
	int exchangeIndices[T*2];
	int ei = 0; // exchange indices counter
	for (int i = 0; i < T; i++) {
		exchangeIndices[ei++] = partitions[i*(T+1) + id];
		exchangeIndices[ei++] = partitions[i*(T+1) + id + 1];
	}
	// k way merge part
	// printArray(exchangeIndices, 2* T);
	// array size
	int mergedLength = 0;
	for (int i = 0; i < T * 2; i+=2) {
		mergedLength += exchangeIndices[i + 1] - exchangeIndices[i];
	}
	
	int* mergedValues = malloc(ISIZE * mergedLength);
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
		free(mergedPartitionLength);
		free(partitions);
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
	
	pthread_barrier_init(&phase1Barrier, NULL, T);
	pthread_barrier_init(&phase2Barrier, NULL, T);
	pthread_barrier_init(&phase3Barrier, NULL, T);
	pthread_barrier_init(&phase4Barrier, NULL, T);
	pthread_barrier_init(&mergingPhaseBarrier, NULL, T);
	
	pthread_t* THREADS = malloc(sizeof(pthread_t) * T);

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
	free(THREADS);
	pthread_barrier_destroy(&phase1Barrier);
	pthread_barrier_destroy(&phase2Barrier);
	pthread_barrier_destroy(&phase3Barrier);
	pthread_barrier_destroy(&phase4Barrier);
	pthread_barrier_destroy(&mergingPhaseBarrier);
	return 0;
}
