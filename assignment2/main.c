#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "helper.h"

#define DEBUG if (1 != 1)
#define ROOT 0
#define MASTER if (rank == ROOT)
#define SLAVE else

int* partition;
int* localRegularSamples;
int* regularSamples;
int* pivots;
int* splitters;
int* lengths;
int* obtainedKeys;
int* mergedArray;
int obtainedKeysSize = 0;

static inline void send(int* data, int size, int dest) {
	MPI_Send(data, size, MPI_INT, dest, 0, MPI_COMM_WORLD);
}

static inline void recv(int* buffer, int bufferSize, int src) {
	MPI_Recv(buffer, bufferSize, MPI_INT, src, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

void phase_1(int partitionSize, int T, int W) {
	// Phase 1: Sorting local data
	qsort(partition, partitionSize, bytes(1), cmpfunc);
	
	// Phase 1: Regular sampling
	localRegularSamples = intAlloc(T);
	for (int i = 0, ix = 0; i < T; i++) {
		localRegularSamples[ix++] = partition[i * W];
	}
}

void phase_2(int rank, int T, int RO) {
	// Phase 2: Sending samples to master
	MASTER {
		regularSamples = intAlloc(T * T); 
		for (int i = 1; i < T; i++) {
			recv(regularSamples + T * i, T, i);
		}
		memcpy(regularSamples, localRegularSamples, bytes(T)); 
	} SLAVE {
		send(localRegularSamples, T, ROOT);
	}
	free(localRegularSamples);
	// Phase 2: Sorting samples and picking pivots
	pivots = intAlloc(T - 1);
	MASTER {
		qsort(regularSamples, T * T, bytes(1), cmpfunc);
		for (int i = 1, ix = 0; i < T; i++) {
			int pos = T * i + RO - 1;
			pivots[ix++] = regularSamples[pos];
		}
		free(regularSamples);
	}
	// Phase 2: Send pivots to all workers/slaves
	MPI_Bcast(pivots, T - 1, MPI_INT, 0, MPI_COMM_WORLD); 
}

void phase_3(int rank, int T, int partitionSize) {
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
	// Phase 3: Sharing array pieces
	// Phase 3: Sharing lengths of those pieces (because other nodes need to allocate memory for it)
	lengths = intAlloc(T);
	lengths[rank] = splitters[rank + 1] - splitters[rank];
	for (int i = 0; i < T; i++) {
		if (rank != i) {
			int length = splitters[i + 1] - splitters[i];
			send(&length, 1, i);
		} else {
			for (int j = 0; j < T; j++) {
				if (rank == j) continue;
				recv(&partitionSize, 1, j);
				lengths[j] = partitionSize;
			}
		}
	}
	// Phase 3: Finding the total size of the array 
	// to place all the acquired pieces
	// int obtainedKeysSize = 0;
	for (int i = 0; i < T; i++) {
		obtainedKeysSize += lengths[i];
	}
 	
	// Phase 3: Exchanging of actual array pieces	
	obtainedKeys = intAlloc(obtainedKeysSize);
	for (int i = 0; i < T; i++) {
                if (rank != i) {
                        int length = splitters[i + 1] - splitters[i];
			send(partition + splitters[i], length, i);
                } else {
                        for (int j = 0; j < T; j++) {
                                if (rank == j) {
					int offset = 0;
					for (int k = 0; k < j; k++) offset += lengths[k];
					memcpy(obtainedKeys + offset, partition + splitters[j], bytes(lengths[j]));
				} else {
					int* keysReceived = intAlloc(lengths[j]);
					recv(keysReceived, lengths[j], j);                                
	
					int pos = 0;
        				for (int k = 0; k < j; k++) {
                				pos += lengths[k];
        				}
					memcpy(obtainedKeys + pos, keysReceived, bytes(lengths[j]));
					free(keysReceived);
				}
                        }
                }
        }
	free(partition);
	free(splitters);
}

void phase_4(int T) {
	// Phase 4: Merging obtained keys
	// Phase 4: Creating the indices array for those pieces (to help merging)
	int* indices = intAlloc(T * 2);
	indices[0] = 0;
	indices[T * 2 - 1] = obtainedKeysSize;
	int localSum = 0;
	for (int i = 1, x = 0; i < T * 2 - 1; i+=2) {
		localSum += lengths[x++];
		indices[i] = localSum;
		indices[i+1] = localSum;
	}
	
	// Phase 4: Merging
	mergedArray = intAlloc(obtainedKeysSize);
	int mi = 0;
	while (mi < obtainedKeysSize) {
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
		mergedArray[mi++] = min;
		indices[pos]++;
	}
	free(obtainedKeys);
	free(indices);
	// Phase 4: Done, PSRS Done!
}

int main(int argc, char *argv[]) {
	int SIZE = atoi(argv[1]);

	MPI_Init(NULL, NULL);

	// how many processors are available
	int T; MPI_Comm_size(MPI_COMM_WORLD, &T);
	
	int perProcessor = SIZE / T;
	int W = SIZE / (T * T);
	int RO = T / 2; 
	
	// what's my rank?
	int rank; MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	// Phase 0: Data distribution
	int partitionSize = (rank == T - 1) ? SIZE - (T - 1) * perProcessor : perProcessor;
	partition = intAlloc(partitionSize);
	MASTER {
		int* DATA = generateArrayDefault(SIZE);
		memcpy(partition, DATA, bytes(perProcessor));
		for (int i = 1; i < T; i++) {
			send(DATA + i * perProcessor, partitionSize, i);
		}
	} SLAVE {
		recv(partition, partitionSize, ROOT);
	}
	// PHASE 1
	phase_1(partitionSize, T, W);
	// PHASE 2
	phase_2(rank, T, RO);
	// PHASE 3
	phase_3(rank, T, partitionSize);
	// PHASE 4
	phase_4(T);	
	// determining the individual lengths of the final array
	MASTER {
		lengths = realloc(lengths, T);
		lengths[0] = obtainedKeysSize;
		for (int i = 1; i < T; i++) {
			int length;
			recv(&length, 1, i);
			lengths[i] = length;
		}
	} SLAVE {
		send(&obtainedKeysSize, 1, ROOT);
		free(lengths);
	}
	
	MASTER {
		int* FINAL = intAlloc(SIZE);
		memcpy(FINAL, mergedArray, bytes(obtainedKeysSize));
		free(mergedArray);
		for (int i = 1; i < T; i++) {
			int* array = intAlloc(lengths[i]);
			recv(array, lengths[i], i);
			int offset = 0;
			for (int x = 0; x < i; x++) offset += lengths[x];
			memcpy(FINAL + offset, array, bytes(lengths[i]));
			free(array);
		}
		isSorted(FINAL, SIZE);
		free(lengths);
		free(FINAL);
	} SLAVE {
		send(mergedArray, obtainedKeysSize, ROOT);
		free(mergedArray);
	}

	MPI_Finalize();
}
