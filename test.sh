#!/bin/bash

types=(1 4)
#num_threads=(2)
num_threads=(2 4 6 8 12 16 24 32 48 64)
TEST="H_MPMCL2_LatencyTest"
RAW="H_MPMCL2_LatencyData"
samples=131072
interval=10

for type in "${types[@]}"
do	
	for threads in "${num_threads[@]}"
	do
		echo "Benchmarking $type with $threads thread(s)"
		./main ${type} ${threads} ${TEST} ${RAW}_${type}_${threads} ${samples} ${interval} 
		echo ""
	done
done 
