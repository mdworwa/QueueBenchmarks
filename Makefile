CC = g++ -g
CFLAGS = -std=c++14 -pthread
#OBJECTS = HazardPointers.o HazardPointersConditional.o waitfree.o Main.o
#Main.o: Main.cpp HazardPointers.hpp HazardPointersConditional.hpp waitfree.hpp
#	$(CC) $(CFLAGS) -c Main.cpp
#main: Main.o
	#$(CC) $(CFLAGS) -o $(OBJECTS) main

all: 
	$(CC) $(CFLAGS) -DTICKS -DLATENCY HazardPointers.hpp HazardPointersConditional.hpp waitfree.hpp fifo.c squeue.c Main.cpp -lpthread -lrt -o main
	
clean:
	rm -f *.o main
	