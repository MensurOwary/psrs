#include <mpi.h>
#include <stdio.h>

int main() {
	MPI_Init(NULL, NULL);
	
	int worldSize;
	MPI_Comm_size(MPI_COMM_WORLD, &worldSize);

	int worldRank;
	MPI_Comm_rank(MPI_COMM_WORLD, &worldRank);
	
	char processorName[MPI_MAX_PROCESSOR_NAME];
	int nameLen;
	MPI_Get_processor_name(processorName, &nameLen);

	printf("Hello from p %s, rank %d, total %d\n", 
		processorName, worldRank, worldSize); 
	
	MPI_Finalize();
}
