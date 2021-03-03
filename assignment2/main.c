#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROOT 0
#define MASTER if (rank == ROOT)
#define ISIZE sizeof(int)

int* generateArrayDefault() {
	int arr[36] = {16, 2, 17, 24, 33, 28, 30, 1, 0, 27, 9, 25, 34, 23, 19, 18, 11, 7, 21, 13, 8, 35, 12, 29, 6, 3, 4, 14, 22, 15, 32, 10, 26, 31, 20, 5};

	int* randoms = malloc(ISIZE * 36);
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

void send(int* arr, int size, int dest) {
	MPI_Send(arr, size, MPI_INT, dest, 0, MPI_COMM_WORLD);
}

void recv(int* arr, int size, int source) {
	printf("%d\n", size);
	MPI_Recv(arr, size, MPI_INT, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
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
	int* partition = malloc(ISIZE * perProcessor);
	MASTER {
		memcpy(partition, DATA, ISIZE * perProcessor);
		for (int i = 1; i < T; i++) {
			int* start = DATA + (i * perProcessor);
			send(start, perProcessor, i);
		}
	} else {
		recv(partition, perProcessor, ROOT);
	}
	// Phase 1: Sorting local data
	qsort(partition, perProcessor, ISIZE, cmpfunc);
	
	// Phase 1: Regular sampling
	/* regular sampling */
	int ix = 0;
	int* localRegularSamples = malloc(ISIZE * T);
	for (int i = 0; i < T; i++) {
		localRegularSamples[ix++] = partition[i * W];
	}
	// Phase 2: Sending samples to master
	int* regularSamples = malloc(ISIZE * T * T);
	MASTER {
		for (int i = 1; i < T; i++) {
			int* start = regularSamples + (i * T);
			recv(start, T, i);
		}
		memcpy(regularSamples, localRegularSamples, ISIZE * T); 
	} else {
		send(localRegularSamples, T, ROOT); 
	}
	printf("%d) Phase 2 sending samples done\n", rank);
	// Phase 2: Sorting and picking pivots
	int* pivots = malloc(ISIZE * (T - 1));
	MASTER {
		qsort(regularSamples, T * T, ISIZE, cmpfunc);
		for (int i = 1, ix = 0; i < T; i++) {
			int pos = T * i + RO - 1;
			pivots[ix++] = regularSamples[pos];
		}
	}
	MPI_Bcast(pivots, T - 1, MPI_INT, 0, MPI_COMM_WORLD); 
	printArray(pivots, T - 1);
	// Phase 3:
	// Phase 4:

	MPI_Finalize();
}
