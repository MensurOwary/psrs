#!/bin/bash
# this script will run the sorting algorithm with the given arguments
# and save the output of each run to data/out{thread_count}-{load_size}-{time}.txt

KEYS=( 16000000 32000000 64000000 128000000 256000000 512000000 )

mkdir -p data

make compile

for thread_count in 2 4 6 8
do
	for size in "${KEYS[@]}"
	do
		for time in {1..5}
		do
			echo "Running ${thread_count} : ${size} - $time time"
			./main.o $size $thread_count > ./data/out"$thread_count-$size-$time".txt
		done
	done
done
