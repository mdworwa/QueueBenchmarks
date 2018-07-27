#!/bin/bash

types=(4)
#num_threads=(2)
num_threads=(2 4 6 8 12 16 24 32 48 64)
TEST="S_MPMC_LatencyTest"
RAW="S_MPMC_LatencyData"
samples=33554432
interval=1000

for type in "${types[@]}"
do	
	for threads in "${num_threads[@]}"
	do
		echo "Benchmarking $type with $threads thread(s)"
		./main ${type} ${threads} ${TEST} ${RAW}_${type}_${threads} ${samples} ${interval} 
		echo ""
	done
done 
