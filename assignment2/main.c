#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "helper.h"

#define DEBUG if (1 != 1)
#define CHECK_SORTED if (1 != 1)
#define ROOT 0
#define MASTER if (rank == ROOT)
#define SLAVE else

// partition that a processor gets
int* partition;
int* localRegularSamples;
int* regularSamples;
int* pivots;
// splitting indices
int* splitters;
// lengths of those split array pieces
int* lengths;
// obtained keys from other processors
int* obtainedKeys;
// locally merged array
int* mergedArray;

// the actual array that neeeds to be sorted
int* DATA;

int obtainedKeysSize = 0;
int partitionSize;
int T;
int SIZE;
int W;
int RO;
int rank;

// checks if the array is sorted
void checkSorted(int* arr) {
	qsort(DATA, SIZE, bytes(1), cmpfunc);
	isSorted(arr, SIZE);
	for (int i = 0; i < SIZE; i++) {
		if (DATA[i] != arr[i]) {
			printf("Key by key - not sorted\n");
			return;
		}
	}
	printf("Key by Sorted\n");
}

// data distribution phase
void phase_0() {
	// regular key size that a processor will get
	int perProcessor = SIZE / T;
	// the actual key size that the processor will get
	partitionSize = (rank == T - 1) ? SIZE - (T - 1) * perProcessor : perProcessor;
	// allocate sufficient memory for the local array
	partition = intAlloc(partitionSize);
	
	int* partitionLengths = NULL;
	int* partitionDisplacement = NULL;
	MASTER {
		DATA = generateArrayDefault(SIZE);
		// array sizes that each processor will get
		partitionLengths = intAlloc(T);
		for (int aRank = 0, l = 0; aRank < T; aRank++) {
			partitionLengths[l++] = (aRank == T - 1) ? SIZE - (T - 1) * perProcessor : perProcessor;
		}
		partitionDisplacement = createPositions(partitionLengths, T);
	}

	MPI_Scatterv(DATA, partitionLengths, partitionDisplacement, MPI_INT, partition, partitionSize, MPI_INT, ROOT, MPI_COMM_WORLD);
	
	free(partitionLengths);
	free(partitionDisplacement);
}

// local sorting and regular sampling phase
void phase_1() {
	// Sorting local data
	qsort(partition, partitionSize, bytes(1), cmpfunc);
	
	// Regular sampling
	localRegularSamples = intAlloc(T);
	for (int i = 0, ix = 0; i < T; i++) {
		localRegularSamples[ix++] = partition[i * W];
	}
	printf("[%d] sorted %d elements\n", rank, partitionSize);
}

// pivot selection phase
void phase_2() {
	// Sending samples to master
	MASTER {
		regularSamples = intAlloc(T * T); 
	}
	// get all the local samples
	MPI_Gather(localRegularSamples, T, MPI_INT, regularSamples, T, MPI_INT, ROOT, MPI_COMM_WORLD);
	free(localRegularSamples);

	// Sorting samples and picking pivots
	pivots = intAlloc(T - 1);
	MASTER {
		qsort(regularSamples, T * T, bytes(1), cmpfunc);
		for (int i = 1, ix = 0; i < T; i++) {
			int pos = T * i + RO - 1;
			pivots[ix++] = regularSamples[pos];
		}
		free(regularSamples);
	}
	// Send pivots to all workers/slaves
	MPI_Bcast(pivots, T - 1, MPI_INT, 0, MPI_COMM_WORLD); 
}

// split pieces exchange phase
void phase_3() {
	// Phase 3: Finding splitting positions
	splitters = intAlloc(T + 1);
	splitters[0] = 0;
	splitters[T] = partitionSize;
	for (int i = 0, pc = 1, pi = 0; i < partitionSize && pi != T - 1; i++) {
		if (pivots[pi] < partition[i]) {
			splitters[pc] = i;
			pc++; pi++;
		}
	}
	free(pivots);
	// Sharing lengths of those pieces (because other nodes need to allocate memory for it)
	int* pieceLengths = intAlloc(T);
	for (int i = 0; i < T; i++) pieceLengths[i] = splitters[i+1] - splitters[i];

	lengths = intAlloc(T);	
	MPI_Alltoall(pieceLengths, 1, MPI_INT, lengths, 1, MPI_INT, MPI_COMM_WORLD);
	
	// Sharing array pieces
	int* positionsSend = createPositions(pieceLengths, T);
	int* positionsRecv = createPositions(lengths, T);

	obtainedKeysSize = 0;
	for (int i = 0; i < T; i++) obtainedKeysSize += lengths[i];
	obtainedKeys = intAlloc(obtainedKeysSize);
	
	MPI_Alltoallv(partition, pieceLengths, positionsSend, MPI_INT, obtainedKeys, lengths, positionsRecv, MPI_INT, MPI_COMM_WORLD);
 	
	free(pieceLengths);
	free(positionsSend);
	free(positionsRecv);	
	free(partition);
	free(splitters);
}

// given an array of indices, 
// it returns the first valid index
// 
// since indices is an array that contains the range
// each time an element from that range is picked as
// current minimum, its starting index gets incremented by 1
// which means when the lower index reaches the high bound
// it should no longer be considered
//
// elements in indices array are places as
// (r1start, r1end, r2start, r2end, ..., rnstart, rnend) where n is T
int findInitialMinPos(int * indices, int size) {
	for (int i = 0; i < size; i+=2)
		if (indices[i] != indices[i+1]) return i;
	return -1;
}

// local merge phase
void phase_4() {
	// Merging obtained keys
	// Creating the indices array for those pieces (to help merging)
	int* indices = intAlloc(T * 2);
	indices[0] = 0;
	indices[T * 2 - 1] = obtainedKeysSize;
	int localSum = 0;
	for (int i = 1, x = 0; i < T * 2 - 1; i+=2) {
		localSum += lengths[x++];
		indices[i] = localSum;
		indices[i+1] = localSum;
	}
	
	// Merging
	mergedArray = intAlloc(obtainedKeysSize);
	for (int mi = 0; mi < obtainedKeysSize; mi++) {
		int pos = findInitialMinPos(indices, T * 2);
		if (pos == -1) break;
		int min = obtainedKeys[indices[pos]];
		for (int i = 0; i < T * 2; i+=2) {
			if (indices[i] != indices[i+1]) {
				if (obtainedKeys[indices[i]] < min) {
					min = obtainedKeys[indices[i]];
					pos = i;
				}
			}
		}
		mergedArray[mi] = min;
		indices[pos]++;
	}
	free(obtainedKeys);
	free(indices);
	free(lengths);
	printf("[%d] merged %d keys\n", rank, obtainedKeysSize);
	// PSRS Done!
}

// merging all the local arrays 
// into one single array in master node
void phase_merge() {
	// determining the individual lengths of the final array
	MASTER {
		lengths = intAlloc(T);
	}

	MPI_Gather(&obtainedKeysSize, 1, MPI_INT, lengths, 1, MPI_INT, ROOT, MPI_COMM_WORLD);
	

	// getting arrays from workers 
	int* FINAL = NULL;
	int* displacements = NULL;
	MASTER {
		FINAL = intAlloc(SIZE);
		displacements = createPositions(lengths, T);
	} SLAVE {
		lengths = NULL;
	}
	
	MPI_Gatherv(mergedArray, obtainedKeysSize, MPI_INT, FINAL, lengths, displacements, MPI_INT, ROOT, MPI_COMM_WORLD);

	MASTER { CHECK_SORTED checkSorted(FINAL); }
	
	free(displacements);
	free(FINAL);
	free(DATA);
	free(mergedArray);
	free(lengths);
}

void measureTime(void (*fun)(), char* processorName, char* title, int shouldLog) {
	if (shouldLog) {
		struct timeval* start = getTime();
		fun();
		long int time = endTiming(start);
		printf("[%s:%d] %s took %ld ms\n", processorName, rank, title, time);
	} else {
		fun();
	}
}

int main(int argc, char *argv[]) {
	MPI_Init(&argc, &argv);

	// how many processors are available
	MPI_Comm_size(MPI_COMM_WORLD, &T);
	
	SIZE = atoi(argv[1]);
	W = SIZE / (T * T);
	RO = T / 2; 
	
	// what's my rank?
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	
	char processor_name[MPI_MAX_PROCESSOR_NAME];
    	int name_len;
    	MPI_Get_processor_name(processor_name, &name_len);

	// Phase 0: Data distribution
	measureTime(phase_0, processor_name, "Phase 0", rank == 0);
	MPI_Barrier(MPI_COMM_WORLD);	
	struct timeval* start = getTime();
	// PHASE 1
	measureTime(phase_1, processor_name, "Phase 1", 1);
	// PHASE 2
	measureTime(phase_2, processor_name, "Phase 2", 1);
	// PHASE 3
	measureTime(phase_3, processor_name, "Phase 3", 1);
	// PHASE 4
	measureTime(phase_4, processor_name, "Phase 4", 1);
	
	MPI_Barrier(MPI_COMM_WORLD);
	long int time = endTiming(start);
	// PHASE Merge	
	measureTime(phase_merge, processor_name, "Phase Merge", rank == 0);
	
	MASTER printf("Complete execution took: %ld ms\n", time);
	
	MPI_Finalize();
}
