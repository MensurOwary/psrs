#!/bin/bash

KEYS=( 1000000 16000000 32000000 64000000 128000000 256000000 )

mkdir -p data_p2p

make

for process_count in 2 4 6 8
do
	for size in "${KEYS[@]}"
	do
		echo "Running ${process_count} : ${size}"
		{
		for time in {1..5}
		do
			mpirun -n $process_count -f hosts ./a.out $size
		done
		} > ./data_p2p/out-"$process_count-$size".txt
	done
done
