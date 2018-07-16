types=(2)
num_threads=(2 4 6 8 12 16 24 32 48 64)
TEST="WFTests"
RAW="WFData"

for type in "${types[@]}"
do
	for threads in "${num_threads[@]}"
	do
		echo "Benchmarking $type with $threads thread(s)"
		./main ${type} ${threads} ${TEST} ${RAW}
		echo ""
	done
done 
