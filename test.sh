#!/bin/bash

types=(1 2)
num_threads=(2 4 6 8 12 16 24 32 48 64)
TEST="LatencyTest"
RAW="LatencyData"
samples=0
interval=0

for type in "${types[@]}"
do	
	if [ $type -eq 1 ]
	then
		samples=67108864
		interval=1000 
	else
		samples=8388608
		interval=100
	fi
	for threads in "${num_threads[@]}"
	do
		echo "Benchmarking $type with $threads thread(s)"
		./main ${type} ${threads} ${TEST} ${RAW}_${type}_${threads} ${samples} ${interval} 
		echo ""
	done
done 
