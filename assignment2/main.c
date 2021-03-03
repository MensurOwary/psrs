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

int main() {
	int* DATA = generateArrayDefault();

	MPI_Init(NULL, NULL);
	
	// how many processors are available
	int T;
	MPI_Comm_size(MPI_COMM_WORLD, &T);
	
	// what's my order?
	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	
	// if I am the master
	int* partition = malloc(sizeof(int) * (36/T));
	int perProcessor = 36 / T;
	if (rank == 0) {
		memcpy(partition, DATA, sizeof(int) * perProcessor);
		for (int i = 1; i < T; i++) {
			int* start = DATA + (i * perProcessor);
			MPI_Send(start, perProcessor, MPI_INT, i, 0, MPI_COMM_WORLD);
		}
	} else {
		MPI_Recv(partition, perProcessor, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	}
	for (int i = 0; i < perProcessor; i++) {
		printf("%d ", partition[i]);
	}
	printf("\n");
	free(partition);

	// name of the processor
	char processorName[MPI_MAX_PROCESSOR_NAME];
	int nameLen;
	MPI_Get_processor_name(processorName, &nameLen);

	printf("Hello from p %s, rank %d, total %d\n", 
		processorName, rank, T); 
	
	MPI_Finalize();
}
