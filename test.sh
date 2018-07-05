types=(1 3)
num_threads=(2 4 6 8 12 16 24 32 48 64)
TEST="LatencyTests"

for type in "${types[@]}"
do
	for threads in "${num_threads[@]}"
	do
		echo "Benchmarking $type with $threads thread(s)"
		./main ${type} ${threads} ${TEST}
		echo ""
	done
done 
