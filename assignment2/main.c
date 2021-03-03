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
			MPI_Send(start, perProcessor, MPI_INT, i, 0, MPI_COMM_WORLD);
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
	// Phase 1: Sending samples to master
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
	// Phase 2:
	if (rank == 0) {
		for (int i = 0; i < T * T; i++) {
			printf("%d ", regularSamples[i]);
		}
		printf("\n");
	} 
	// Phase 3:
	// Phase 4:

	MPI_Finalize();
}
