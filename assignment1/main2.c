#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <limits.h>

#define SIZE 39
#define T 4
#define W (SIZE/(T*T))
#define RO (T/2)

int cmpfunc (const void * a, const void * b) {
	return ( *(int *) a - *(int*)b );
}

struct thread_data {
	int id;
	int start;
	int end;
};

pthread_barrier_t barrier;
pthread_barrier_t phase2Barrier;
pthread_barrier_t phase3Barrier;
pthread_barrier_t phase4Barrier;
pthread_barrier_t mergingPhaseBarrier;

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

void printArray(int* array, int size) {
	for (int i = 0; i < size; i++) {
		printf("%d ", array[i]);
	}
	printf("\n");
}

int* generateArrayDefault() {
	int arr[36] = {16, 2, 17, 24, 33, 28, 30, 1, 0, 27, 9, 25, 34, 23, 19, 18, 11, 7, 21, 13, 8, 35, 12, 29, 6, 3, 4, 14, 22, 15, 32, 10, 26, 31, 20, 5};
	
	int* randoms = malloc(sizeof(int) * 36);
	for (int i = 0; i < 36; i++) {
		randoms[i] = arr[i];
	}
	return randoms;
}

int shouldContinueMerging(int * exchangeIndices, int size) {
	for (int i = 0; i < size; i+=2) {
		if (exchangeIndices[i] != exchangeIndices[i+1]) {
			return 1;
		}
	}
	return 0;
}

int regularSamples[T*T];
int pivots[T - 1];
int partitions[T][T+1];
int mergedPartitionLength[T]; // each thread will save its length here

void* psrs(void *args) {
	struct thread_data* data = (struct thread_data*) args;
	/* Phase 1 */
	qsort((INPUT + data->start), (data->end - data->start), sizeof(int), cmpfunc);
	
	/* regular sampling */
	int ix = 0;
	for (int i = 0; i < T; i++) {
		regularSamples[data->id * T + ix] = INPUT[data->start + (i * W)];
		ix++;
	}	

	pthread_barrier_wait(&barrier);
	/* Phase 2 */
	if (data->id == 0) { // inside the master thread
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
	partitions[data->id][0] = data->start;
	partitions[data->id][T] = data->end;
	for (int i = data->start; i < data->end && pi != T-1; i++) {
		if (pivots[pi] < INPUT[i]) {
			partitions[data->id][pc++] = i;
			pi++;
		}
	}
	
	// for (int i = 0; i < T + 1; i++) {
	//	printf("(T%d) : %d ", data->id, partitions[data->id][i]);
	// }
	// printf("\n");
	pthread_barrier_wait(&phase3Barrier);
	/* Phase 4 */
	int exchangeIndices[T*2];
	int ei = 0;
	for (int i = 0; i < T; i++) {
		exchangeIndices[ei++] = partitions[i][data->id];
		exchangeIndices[ei++] = partitions[i][data->id+1];
	}
	// k way merge part
	// printArray(exchangeIndices, 2* T);
	// array size
	int mergedLength = 0;
	for (int i = 0; i < T * 2; i+=2) {
		mergedLength += exchangeIndices[i + 1] - exchangeIndices[i];
	}
	
	int* mergedValues = malloc(sizeof(int) * mergedLength);
	mergedPartitionLength[data->id] = mergedLength;
	int mi = 0;
	// printf("Created an array of len %d\n", mergedLength);
	while (shouldContinueMerging(exchangeIndices, T * 2)) {
		// find minimum among current items
		int min = INT_MAX;
		int minPos = -1;
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
	printf("Thread %d waits\n", data->id);
	pthread_barrier_wait(&phase4Barrier);
	
	int startPos = 0;
	int x = data->id - 1;
	while (x >= 0) {
		startPos += mergedPartitionLength[x--];
	}
	for (int i = startPos; i < startPos + mergedLength; i++) {
		INPUT[i] = mergedValues[i - startPos];
	}
	free(mergedValues);
	pthread_barrier_wait(&mergingPhaseBarrier);
}


int main(){
	printf("Size : %d\n", SIZE);
	INPUT = generateArrayOfSize(SIZE);
	// printArray(INPUT, SIZE);
	int perThread = SIZE / T;
	
	pthread_barrier_init(&barrier, NULL, T);
	pthread_barrier_init(&phase2Barrier, NULL, T);
	pthread_barrier_init(&phase3Barrier, NULL, T);
	pthread_barrier_init(&phase4Barrier, NULL, T);
	pthread_barrier_init(&mergingPhaseBarrier, NULL, T);
	pthread_t THREADS[T];

	for (int i = 1; i < T; i++) {
		struct thread_data* data = malloc(sizeof(struct thread_data));
		data->start = i * perThread;
		data->end = data->start + perThread;
		data->id = i;
		pthread_create(&THREADS[i], NULL, psrs, (void *) data);
	}
	
	struct thread_data* data = malloc(sizeof(struct thread_data));
	data->start = 0;
	data->end = perThread;
	data->id = 0;
	psrs((void *) data);
	
	printf("Threads finished\n");

	printArray(INPUT, SIZE);
	isSorted();
	return 0;
}
