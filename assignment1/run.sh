#!/bin/bash

KEYS=( 16000000 32000000 64000000 128000000 256000000 512000000 )

mkdir -p data2

make compile

for thread_count in 2 3 4 5 6
do
	for size in "${KEYS[@]}"
	do
		for time in {1..5}
		do
			echo "Running ${thread_count} : ${size} - $time time"
			./main.o $size $thread_count > ./data2/out"$thread_count-$size-$time".txt
		done
	done
done
