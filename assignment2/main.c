#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int* generateArrayDefault() {
	int arr[36] = {16, 2, 17, 24, 33, 28, 30, 1, 0, 27, 9, 25, 34, 23, 19, 18, 11, 7, 21, 13, 8, 35, 12, 29, 6, 3, 4, 14, 22, 15, 32, 10, 26, 31, 20, 5};

	int* randoms = malloc(sizeof(int) * 36);
	for (int i = 0; i < 36; i++) {
		randoms[i] = arr[i];
	}
	return randoms;
}

int cmpfunc (const void * a, const void * b) { return ( *(int *) a - *(int*)b );}

void printArray(int* a, int size) {
	for (int i = 0; i < size; i++) printf("%d ", a[i]);
	printf("\n");
}

int main() {
	// generate data
	int* DATA = generateArrayDefault();
	int SIZE = 36;

	MPI_Init(NULL, NULL);
	
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
	if (rank == 0) {
		memcpy(partition, DATA, sizeof(int) * perProcessor);
		for (int i = 1; i < T; i++) {
			int* start = DATA + (i * perProcessor);
			int partitionSize = perProcessor;
			if (rank == T - 1) {
				partitionSize = SIZE - (T - 1) * perProcessor;
			}
			MPI_Send(start, partitionSize, MPI_INT, i, 0, MPI_COMM_WORLD);
		}
	} else {
		MPI_Recv(partition, perProcessor, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	}
	// Phase 1: Sorting local data
	qsort(partition, perProcessor, sizeof(int), cmpfunc);
	
	// Phase 1: Regular sampling
	/* regular sampling */
	int ix = 0;
	int* localRegularSamples = malloc(sizeof(int) * T);
	for (int i = 0; i < T; i++) {
		localRegularSamples[ix++] = partition[i * W];
	}
	// Phase 2: Sending samples to master
	int* regularSamples = malloc(sizeof(int) * T * T);
	if (rank == 0) {
		for (int i = 1; i < T; i++) {
			int* start = regularSamples + (i * T);
			MPI_Recv(start, T, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		}
		memcpy(regularSamples, localRegularSamples, sizeof(int) * T); 
	} else {
		MPI_Send(localRegularSamples, T, MPI_INT, 0, 0, MPI_COMM_WORLD); 
	}
	printf("%d) Phase 2 sending samples done\n", rank);
	// Phase 2: Sorting and picking pivots
	int* pivots = malloc(sizeof(int) * (T - 1));
	if (rank == 0) {
		qsort(regularSamples, T * T, sizeof(int), cmpfunc);
		for (int i = 1, ix = 0; i < T; i++) {
			int pos = T * i + RO - 1;
			pivots[ix++] = regularSamples[pos];
		}
	}
	MPI_Bcast(pivots, T - 1, MPI_INT, 0, MPI_COMM_WORLD); 
	// Phase 3:
	int partitionSize = rank == T - 1 ? (SIZE - (T - 1) * perProcessor) : perProcessor; 
	int* splitters = malloc(sizeof(int) * (T - 1 + 2));
	splitters[0] = 0;
	splitters[T] = partitionSize;
	for (int i = 0, pc = 1, pi = 0; i < partitionSize; i++) {
		if (pivots[pi] < partition[i]) {
			splitters[pc] = i;
			pc++; pi++;
		}
	}
	printf("%d: ", rank);
	printArray(splitters, (T - 1 + 2));
	
	int* lengths = malloc(sizeof(int) * T);
	lengths[rank] = splitters[rank + 1] - splitters[rank];
	for (int i = 0; i < T; i++) {
		int start = splitters[i];
		int end = splitters[i + 1];
		if (rank != i) {
			int length = end - start;
			MPI_Send(&length, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
			// MPI_Send(partitions + start, (end - start), MPI_INT, i, 0, MPI_COMM_WORLD); 
		} else {
			MPI_Status status;
			MPI_Recv(&partitionSize, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
			lengths[status.MPI_SOURCE] = partitionSize;
		}
	}
	int sum = 0;
	for (int i = 0; i < T; i++) {
		sum += lengths[i];
	}
	printf("%d: phase 3 array size: %d\n", rank, sum);
	printf("%d: ", rank);
	printArray(lengths, T);
 
	// Phase 4:

	MPI_Finalize();
}
