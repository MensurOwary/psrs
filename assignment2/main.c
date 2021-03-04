#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG if (1 != 1)
#define MASTER if (rank == 0)
#define SLAVE else

int* generateArrayDefault(int size) {
	// int arr[36] = {16, 2, 17, 24, 33, 28, 30, 1, 0, 27, 9, 25, 34, 23, 19, 18, 11, 7, 21, 13, 8, 35, 12, 29, 6, 3, 4, 14, 22, 15, 32, 10, 26, 31, 20, 5};
	srandom(15);
	int* randoms = malloc(sizeof(int) * size);
	for (int i = 0; i < size; i++) {
		randoms[i] = (int) random() % 1000; // arr[i];
	}
	return randoms;
}

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

int main(int argc, char *argv[]) {
	// generate data: only in MASTER: this one is wrong!!
	int SIZE = atoi(argv[1]);

	MPI_Init(NULL, NULL);

	// Get the name of the processor
  	// char processor_name[MPI_MAX_PROCESSOR_NAME];
  	// int name_len;
  	// MPI_Get_processor_name(processor_name, &name_len);
	
	// how many processors are available
	int T;
	MPI_Comm_size(MPI_COMM_WORLD, &T);
	
	int perProcessor = SIZE / T;
	int W = SIZE / (T * T);
	int RO = T / 2; 
	
	// what's my rank?
	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	// Phase 0: Data distribution
	int* partition = malloc(sizeof(int) * perProcessor);
	int partitionSize = perProcessor;
	if (rank == T - 1) {
		partitionSize = SIZE - (T - 1) * perProcessor;
	}
	MASTER {
		int* DATA = generateArrayDefault(SIZE);
		memcpy(partition, DATA, sizeof(int) * perProcessor);
		for (int i = 1; i < T; i++) {
			int* start = DATA + (i * perProcessor);
			MPI_Send(start, partitionSize, MPI_INT, i, 0, MPI_COMM_WORLD);
		}
	} SLAVE {
		MPI_Recv(partition, perProcessor, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	}
	// Phase 1: Sorting local data
	qsort(partition, partitionSize, sizeof(int), cmpfunc);
	
	// Phase 1: Regular sampling
	/* regular sampling */
	int ix = 0;
	int* localRegularSamples = malloc(sizeof(int) * T);
	for (int i = 0; i < T; i++) {
		localRegularSamples[ix++] = partition[i * W];
	}
	// Phase 2: Sending samples to master
	int* regularSamples = malloc(sizeof(int) * T * T);
	MASTER {
		for (int i = 1; i < T; i++) {
			int* start = regularSamples + (i * T);
			MPI_Recv(start, T, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		}
		memcpy(regularSamples, localRegularSamples, sizeof(int) * T); 
	} SLAVE {
		MPI_Send(localRegularSamples, T, MPI_INT, 0, 0, MPI_COMM_WORLD); 
	}
	// Phase 2: Sorting and picking pivots
	int* pivots = malloc(sizeof(int) * (T - 1));
	MASTER {
		qsort(regularSamples, T * T, sizeof(int), cmpfunc);
		for (int i = 1, ix = 0; i < T; i++) {
			int pos = T * i + RO - 1;
			pivots[ix++] = regularSamples[pos];
		}
	}
	MPI_Bcast(pivots, T - 1, MPI_INT, 0, MPI_COMM_WORLD); 
	// Phase 3:
	// int partitionSize = rank == T - 1 ? (SIZE - (T - 1) * perProcessor) : perProcessor; 
	printf("%d: ps : %d\n", rank, partitionSize);
	int* splitters = malloc(sizeof(int) * (T - 1 + 2));
	splitters[0] = 0;
	splitters[T] = partitionSize;
	for (int i = 0, pc = 1, pi = 0; i < partitionSize && pi != T - 1; i++) {
		if (pivots[pi] < partition[i]) {
			splitters[pc] = i;
			pc++; pi++;
		}
	}
	
	int* lengths = malloc(sizeof(int) * T);
	lengths[rank] = splitters[rank + 1] - splitters[rank];
	for (int i = 0; i < T; i++) {
		if (rank != i) {
			int length = splitters[i + 1] - splitters[i];
			MPI_Send(&length, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
		} else {
			for (int j = 0; j < T; j++) {
				if (rank == j) continue;
				MPI_Recv(&partitionSize, 1, MPI_INT, j, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				lengths[j] = partitionSize;
			}
		}
	}
	int sum = 0;
	for (int i = 0; i < T; i++) {
		sum += lengths[i];
	}
 	
	int* obtainedKeys = malloc(sizeof(int) * sum);
	for (int i = 0; i < T; i++) {
                if (rank != i) {
                        int length = splitters[i + 1] - splitters[i];
                        MPI_Send(partition + splitters[i], length, MPI_INT, i, 0, MPI_COMM_WORLD);
                } else {
                        for (int j = 0; j < T; j++) {
                                if (rank == j) {
					int pos = 0;
					for (int k = 0; k < j; k++) pos += lengths[k];
					memcpy(obtainedKeys + pos, partition + splitters[j], sizeof(int) * lengths[j]);
					continue;
				}
				int* keysReceived = malloc(sizeof(int) * lengths[j]);
                                MPI_Recv(keysReceived, lengths[j], MPI_INT, j, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                                
				int pos = 0;
        			for (int k = 0; k < j; k++) {
                			pos += lengths[k];
        			}
				memcpy(obtainedKeys + pos, keysReceived, sizeof(int) * lengths[j]);
				free(keysReceived);
                        }
                }
        }
	// Phase 4:
	int* indices = malloc(sizeof(int) * (T * 2));
	indices[0] = 0;
	indices[T * 2 - 1] = sum;
	int localSum = 0;
	for (int i = 1, x = 0; i < T * 2 - 1; i+=2) {
		localSum += lengths[x++];
		indices[i] = localSum;
		indices[i+1] = localSum;
	}
	
	int* mergedArray = malloc(sizeof(int) * sum);
	int mi = 0;
	while (mi < sum) {
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
	
	// determining the individual lengths of the final array
	MASTER {
		lengths = realloc(lengths, T);
		lengths[0] = sum;
		for (int i = 1; i < T; i++) {
			int length;
			MPI_Recv(&length, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			lengths[i] = length;
		}
		printArray(lengths, T);
	} else {
		MPI_Send(&sum, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
	}
	
	int* FINAL = malloc(SIZE * sizeof(int));
	MASTER {
		memcpy(FINAL, mergedArray, sizeof(int) * sum);
		for (int i = 1; i < T; i++) {
			int* array = malloc(sizeof(int) * lengths[i]);
			MPI_Recv(array, lengths[i], MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			int offset = 0;
			for (int x = 0; x < i; x++) offset += lengths[x];
			memcpy(FINAL + offset, array, sizeof(int) * lengths[i]);
		}
		printArray(FINAL, SIZE);
		isSorted(FINAL, SIZE);
	} else {
		MPI_Send(mergedArray, sum, MPI_INT, 0, 0, MPI_COMM_WORLD);
	}

	MPI_Finalize();
}
