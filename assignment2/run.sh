#!/bin/bash

KEYS=( 1000000 16000000 32000000 64000000 128000000 256000000 )

mkdir -p data

make

for process_count in 2 4 6 8
do
	for size in "${KEYS[@]}"
	do
		for time in {1..5}
		do
			echo "Running ${process_count} : ${size} - $time time"
			mpirun -n $process_count ./a.out $size > ./data/out-"$process_count-$size-$time".txt
		done
	done
done
