type=(1 2 3)
num_threads=(2 4 6 8 12 16 24 32 48 64)

for type in "${types}"
	for threads in "${num_threads[@]}"
		do
			echo "Benchmarking $type with $threads thread(s)"
			./main ${type} ${threads}
			echo ""
		done 
