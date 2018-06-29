CC = g++ -g
CFLAGS = -std=c++14 -pthread

all: 
	$(CC) $(CFLAGS) -DTICKS -DLATENCY HazardPointers.hpp HazardPointersConditional.hpp waitfree.hpp fifo.c squeue.c Main.cpp -lpthread -lrt -o main
		
clean:
	rm -f *.o main