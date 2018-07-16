/*
 * Main.cpp
 *
 *  Created on: Jun 15, 2018
 *      Author: mdworwa
 */
#include <stdio.h>
#include "HazardPointers.hpp"
#include "HazardPointersConditional.hpp"
#include "waitfree.hpp"
#include <pthread.h>
#include <sched.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <omp.h>
#include <time.h>
#include "squeue.h"
#include "fifo.h"

typedef unsigned long long ticks;
#define NUM_THREADS 1
//#define NUM_SAMPLES 67108864 //closest to 100,000,000 but under
//#define NUM_SAMPLES 8388608 //closest to 10,000
#define NUM_SAMPLES 2097152 //works for sure
#define NUM_CPUS 48
#define ENQUEUE_SECONDS 3.0
#define DEQUEUE_SECONDS 3.0

ticks *enqueuetimestamp, *dequeuetimestamp;
static ticks dequeue_ticks = 0, enqueue_ticks = 0;
static int numEnqueue = NUM_SAMPLES;
static int numDequeue = NUM_SAMPLES;
static int CUR_NUM_THREADS = 0;
static pthread_barrier_t barrier;
double enqueuethroughput, dequeuethroughput = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

struct arg_queue {
	KoganPetrankQueueCHP<int> *queue;
	int i;
};

struct threaddata {
	int my_cpu;
	int startIndex;
};

struct queue_t *q;

int enqueueFile[NUM_SAMPLES];
int dequeueFile[NUM_SAMPLES];

#ifdef LATENCY
static __inline__ ticks getticks()
{
	ticks tsc;
	__asm__ __volatile__(
			"rdtsc;"
			"shl $32, %%rdx;"
			"or %%rdx, %%rax"
			: "=a"(tsc)
			  :
			  : "%rcx", "%rdx");

	return tsc;
}
#endif

void *worker_handler(void * in) {
	int my_cpu = (int) (long) in;

	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(my_cpu % NUM_CPUS, &set);

	pthread_setaffinity_np(pthread_self(), sizeof (set), &set);
#ifdef LATENCY
    ticks start_tick, end_tick;
    int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
    ticks *timestamp;
    timestamp = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES_PER_THREAD);
    for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
        timestamp[i] = (ticks) 0;
    }
#endif
#ifdef THROUGHPUT
    struct timespec looptime, loopend;
    struct timespec tstart, tend;
#endif
    pthread_barrier_wait(&barrier);
#ifdef THROUGHPUT
    int ret;
    long int NUM_SAMPLES_PER_THREAD = 0;
    int count = 1;
    double diff = 0.0;
    clock_gettime(CLOCK_MONOTONIC, &looptime);
    clock_gettime(CLOCK_MONOTONIC, &tstart);
    while (diff <= DEQUEUE_SECONDS && ret != -1) {
#endif
#ifdef LATENCY
        for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
            start_tick = getticks();
#endif
#ifdef THROUGHPUT
        ret = Dequeue();
#else
            Dequeue();
#endif
#ifdef LATENCY
            end_tick = getticks();
            timestamp[i] = end_tick - start_tick;
        }
#endif
#ifdef THROUGHPUT
		count++;
		if (count % 10000 == 0 || ret == -1) {
			clock_gettime(CLOCK_MONOTONIC, &loopend);
			NUM_SAMPLES_PER_THREAD += count;
			count = 1;
			diff = (loopend.tv_sec - looptime.tv_sec);
		}
	}
#endif
#ifdef LATENCY
     pthread_mutex_lock(&lock);
     memcpy(dequeuetimestamp + numDequeue, timestamp, NUM_SAMPLES_PER_THREAD * sizeof (ticks));
     numDequeue += NUM_SAMPLES_PER_THREAD;
     pthread_mutex_unlock(&lock);
#endif
#ifdef THROUGHPUT
	clock_gettime(CLOCK_MONOTONIC, &tend);
	pthread_mutex_lock(&lock);
 	double elapsed = (tend.tv_sec - tstart.tv_sec) + ((tend.tv_nsec - tstart.tv_nsec) / 1E9);
	printf("Elapsed time: %lf\n", elapsed);
	printf("Num dequeue tasks run: %ld\n", NUM_SAMPLES_PER_THREAD);
	dequeuethroughput += ((NUM_SAMPLES_PER_THREAD * 1.0) / elapsed);
	pthread_mutex_unlock(&lock);
#endif
	return 0;
}

void *enqueue_handler(void * in) {
	int my_cpu = (int) (long) in;

	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(my_cpu % NUM_CPUS, &set);

	pthread_setaffinity_np(pthread_self(), sizeof (set), &set);
#ifdef LATENCY
    ticks start_tick, end_tick;
    int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
    ticks *timestamp;
    timestamp = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES_PER_THREAD);
    for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
    	timestamp[i] = (ticks) 0;
    }
#endif
#ifdef THROUGHPUT
	struct timespec tstart, tend, looptime, loopend;
	int i = 1;
	long int NUM_SAMPLES_PER_THREAD = 0;
	double diff = 0.0;
#endif
	pthread_barrier_wait(&barrier);
#ifdef THROUGHPUT
	clock_gettime(CLOCK_MONOTONIC, &looptime);
	clock_gettime(CLOCK_MONOTONIC, &tstart);
	while (diff <= ENQUEUE_SECONDS) {
#endif
#ifdef LATENCY
		for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
			start_tick = getticks();
#endif
			Enqueue((atom) i + 1);
#ifdef LATENCY
			end_tick = getticks();
			timestamp[i] = end_tick - start_tick;
		}
#endif
#ifdef THROUGHPUT
		i++;
		if (i % 10000 == 0) {
			clock_gettime(CLOCK_MONOTONIC, &loopend);
			NUM_SAMPLES_PER_THREAD += i;
			i = 1;
			diff = (loopend.tv_sec - looptime.tv_sec);
		}
	}
#endif
#ifdef LATENCY
	pthread_mutex_lock(&lock);
	memcpy(enqueuetimestamp + numEnqueue, timestamp, NUM_SAMPLES_PER_THREAD * sizeof (ticks));
	numEnqueue += NUM_SAMPLES_PER_THREAD;
	pthread_mutex_unlock(&lock);
#endif
#ifdef THROUGHPUT
	clock_gettime(CLOCK_MONOTONIC, &tend);
	pthread_mutex_lock(&lock);
	double elapsed = (tend.tv_sec - tstart.tv_sec)+ ((tend.tv_nsec - tstart.tv_nsec) / 1E9);
	printf("Elapsed time: %lf\n", elapsed);
	printf("Num enqueue tasks run: %ld\n", NUM_SAMPLES_PER_THREAD);
	enqueuethroughput += ((NUM_SAMPLES_PER_THREAD * 1.0) / elapsed);
	pthread_mutex_unlock(&lock);
#endif
    return 0;
}

void *pop_deq(void *input) {
	struct arg_queue* p_queue = (struct arg_queue*) input;
#ifdef LATENCY
	ticks start_tick,end_tick;
	int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
	ticks *timetracker = (ticks*) malloc(sizeof(ticks)*NUM_SAMPLES_PER_THREAD);
	for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
		timetracker[i]=(ticks)0;
	}
#endif
#ifdef THROUGHPUT
	struct timespec looptime, loopend;
	struct timespec tstart, tend;
#endif
	pthread_barrier_wait(&barrier);
#ifdef THROUGHPUT
	long int NUM_SAMPLES_PER_THREAD = 0;
	int count = 1;
	double diff = 0.0;
	clock_gettime(CLOCK_MONOTONIC, &looptime);
	clock_gettime(CLOCK_MONOTONIC, &tstart);
	while (diff <= DEQUEUE_SECONDS) {
#endif
#ifdef LATENCY
		for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
			start_tick = getticks();
#endif
			do {
				int *j = p_queue->queue->dequeue(p_queue->i);
				if(j) {
					break;
				}
			}
			while(true);
#ifdef LATENCY
			end_tick = getticks();
			timetracker[i] = end_tick - start_tick;
		}
#endif
#ifdef THROUGHPUT
		count++;
		if (count % 10000 == 0) {
			clock_gettime(CLOCK_MONOTONIC, &loopend);
			NUM_SAMPLES_PER_THREAD += count;
			count = 1;
			diff = (loopend.tv_sec - looptime.tv_sec);
		}
	}
#endif
#ifdef LATENCY
	pthread_mutex_lock(&lock);
	memcpy(dequeuetimestamp + numDequeue, timetracker, NUM_SAMPLES_PER_THREAD * sizeof (ticks));
	numDequeue += NUM_SAMPLES_PER_THREAD;
	pthread_mutex_unlock(&lock);
#endif
#ifdef THROUGHPUT
	clock_gettime(CLOCK_MONOTONIC, &tend);
	pthread_mutex_lock(&lock);
	double elapsed = (tend.tv_sec - tstart.tv_sec) + ((tend.tv_nsec - tstart.tv_nsec) / 1E9);
	printf("Elapsed time: %lf\n", elapsed);
	printf("Num dequeue tasks run: %ld\n", NUM_SAMPLES_PER_THREAD);
	dequeuethroughput += ((NUM_SAMPLES_PER_THREAD * 1.0) / elapsed);
	pthread_mutex_unlock(&lock);
#endif
	return 0;
}

void *push_enq(void *input) {
	struct arg_queue* p_queue = (struct arg_queue*) input;
#ifdef LATENCY
    ticks start_tick, end_tick;
    int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
    ticks *timetracker;
    timetracker = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES_PER_THREAD);
    for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
    	timetracker[i] = (ticks) 0;
    }
#endif
#ifdef THROUGHPUT
    struct timespec tstart, tend, looptime, loopend;
    int i = 1;
    long int NUM_SAMPLES_PER_THREAD = 0;
    double diff = 0.0;
#endif
    pthread_barrier_wait(&barrier);
#ifdef THROUGHPUT
    clock_gettime(CLOCK_MONOTONIC, &looptime);
    clock_gettime(CLOCK_MONOTONIC, &tstart);
    while (diff <= ENQUEUE_SECONDS) {
#endif
#ifdef LATENCY
    	for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
           start_tick = getticks();
#endif
           int *j = (int*) malloc(sizeof(int));
           *j = (p_queue->i * 100) + i;
           p_queue->queue->enqueue(j, p_queue->i);
#ifdef LATENCY
           end_tick = getticks();
           timetracker[i] = end_tick - start_tick;
    	}
#endif
#ifdef THROUGHPUT
		i++;
		if (i % 10000 == 0) {
			clock_gettime(CLOCK_MONOTONIC, &loopend);
			NUM_SAMPLES_PER_THREAD += i;
			i = 1;
			diff = (loopend.tv_sec - looptime.tv_sec);
		}
    }
#endif
#ifdef LATENCY
       pthread_mutex_lock(&lock);
       memcpy(enqueuetimestamp + numEnqueue, timetracker, NUM_SAMPLES_PER_THREAD * sizeof (ticks));
       numEnqueue += NUM_SAMPLES_PER_THREAD;
       pthread_mutex_unlock(&lock);
#endif
#ifdef THROUGHPUT
       clock_gettime(CLOCK_MONOTONIC, &tend);
       pthread_mutex_lock(&lock);
       double elapsed = (tend.tv_sec - tstart.tv_sec) + ((tend.tv_nsec - tstart.tv_nsec) / 1E9);
       printf("Elapsed time: %lf\n", elapsed);
       printf("Num enqueue tasks run: %ld\n", NUM_SAMPLES_PER_THREAD);
       enqueuethroughput += ((NUM_SAMPLES_PER_THREAD * 1.0) / elapsed);
       pthread_mutex_unlock(&lock);
#endif
       return 0;
}

void *b_worker_handler( void * data){
	struct threaddata* td = (struct threaddata*)data;
#ifdef LATENCY
	ticks start_tick,end_tick,diff_ticks;
	int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
	ticks *timetracker = (ticks*) malloc(sizeof(ticks)*NUM_SAMPLES_PER_THREAD);
	for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
		timetracker[i]=(ticks)0;
	}
#endif
#ifdef THROUGHPUT
	struct timespec looptime, loopend;
	struct timespec tstart, tend;
#endif
	ELEMENT_TYPE value = td->startIndex;
	pthread_barrier_wait(&barrier);
#ifdef THROUGHPUT
	long int NUM_SAMPLES_PER_THREAD = 0;
	int count = 1;
	double diff = 0.0;
	clock_gettime(CLOCK_MONOTONIC, &looptime);
	clock_gettime(CLOCK_MONOTONIC, &tstart);
	while (diff <= DEQUEUE_SECONDS) {
#endif
#ifdef LATENCY
		for (int i = td->startIndex; i < (td->startIndex + NUM_SAMPLES_PER_THREAD); i++) {
			start_tick = getticks();
#endif
//			int ret;
//			do{
//				ret = dequeue(q, &value);
//			}
			//while(ret == 0);
//			while(ret != 0);
			while(dequeue(q, &value) != 0);
#ifdef LATENCY
			dequeueFile[i-1] = (int)value;
			end_tick = getticks();
			diff_ticks = end_tick - start_tick;
			timetracker[i-td->startIndex] = diff_ticks;
			__sync_add_and_fetch(&dequeue_ticks, diff_ticks);
		}
#endif
#ifdef THROUGHPUT
		count++;
		if (count % 10000 == 0) {
			clock_gettime(CLOCK_MOfNOTONIC, &loopend);
			NUM_SAMPLES_PER_THREAD += count;
			count = 1;
			diff = (loopend.tv_sec - looptime.tv_sec);
		}
	}
#endif
#ifdef LATENCY
	pthread_mutex_lock(&lock);
	memcpy(dequeuetimestamp + numDequeue, timetracker, NUM_SAMPLES_PER_THREAD * sizeof (ticks));
	numDequeue += NUM_SAMPLES_PER_THREAD;
	pthread_mutex_unlock(&lock);
#endif
#ifdef THROUGHPUT
	clock_gettime(CLOCK_MONOTONIC, &tend);
	pthread_mutex_lock(&lock);
	double elapsed = (tend.tv_sec - tstart.tv_sec) + ((tend.tv_nsec - tstart.tv_nsec) / 1E9);
	printf("Elapsed time: %lf\n", elapsed);
	printf("Num dequeue tasks run: %ld\n", NUM_SAMPLES_PER_THREAD);
	dequeuethroughput += ((NUM_SAMPLES_PER_THREAD * 1.0) / elapsed);
	pthread_mutex_unlock(&lock);
#endif
	return 0;
}

void *b_enqueue_handler( void * data) {
#ifdef LATENCY
	struct threaddata* td = (struct threaddata*)data;
	ticks start_tick,end_tick,diff_ticks;
	int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
	ticks *timetracker = (ticks*) malloc(sizeof(ticks)*NUM_SAMPLES_PER_THREAD);
	for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
		timetracker[i]=(ticks)0;
	}
#endif
#ifdef THROUGHPUT
	struct timespec looptime, loopend;
	struct timespec tstart, tend;
#endif
	pthread_barrier_wait(&barrier);
#ifdef THROUGHPUT
	long int NUM_SAMPLES_PER_THREAD = 0;
	int count = 1;
	int i = 1;
	double diff = 0.0;
	clock_gettime(CLOCK_MONOTONIC, &looptime);
	clock_gettime(CLOCK_MONOTONIC, &tstart);
	while (diff <= DEQUEUE_SECONDS) {
#endif
#ifdef LATENCY
		for (int i = td->startIndex; i < (td->startIndex + NUM_SAMPLES_PER_THREAD); i++) {
			start_tick = getticks();
#endif
			//while(enqueue(q, (ELEMENT_TYPE)i) != 0);
			int ret;
			do{
				ret = enqueue(q, (ELEMENT_TYPE)i);
			}
			while(ret != 0);
#ifdef LATENCY
			enqueueFile[i-1] = (ELEMENT_TYPE)i;
			end_tick = getticks();
			diff_ticks = end_tick - start_tick;
			timetracker[i-td->startIndex] = diff_ticks;
			__sync_add_and_fetch(&enqueue_ticks, diff_ticks);
		}
#endif
#ifdef THROUGHPUT
		count++;
		i++;
		if (count % 10000 == 0) {
			clock_gettime(CLOCK_MONOTONIC, &loopend);
			NUM_SAMPLES_PER_THREAD += count;
			count = 1;
			diff = (loopend.tv_sec - looptime.tv_sec);
		}
	}
#endif
#ifdef LATENCY
	pthread_mutex_lock(&lock);
	memcpy(enqueuetimestamp + numEnqueue, timetracker, NUM_SAMPLES_PER_THREAD * sizeof (ticks));
	numEnqueue += NUM_SAMPLES_PER_THREAD;
	pthread_mutex_unlock(&lock);
#endif
#ifdef THROUGHPUT
	clock_gettime(CLOCK_MONOTONIC, &tend);
	pthread_mutex_lock(&lock);
	double elapsed = (tend.tv_sec - tstart.tv_sec) + ((tend.tv_nsec - tstart.tv_nsec) / 1E9);
	printf("Elapsed time: %lf\n", elapsed);
	printf("Num enqueue tasks run: %ld\n", NUM_SAMPLES_PER_THREAD);
	dequeuethroughput += ((NUM_SAMPLES_PER_THREAD * 1.0) / elapsed);
	pthread_mutex_unlock(&lock);
#endif
	return 0;
}

int cmpfunc(const void * a, const void * b) {
	return ( *(int*) a - *(int*) b);
}

void SortTicks(ticks* numTicks, int total, int faileddeq) {
	qsort(numTicks, total, sizeof (*numTicks), cmpfunc);
}

void sortArray(int pArray[], int total){
	qsort(pArray, total, sizeof (*pArray), cmpfunc);
}

bool check(int array1[], int array2[]){
	for(int i=0; i<NUM_SAMPLES; i++){
		if(array1[i]!=array2[i]){
			return false;
		}
	}
	return true;
}

void ComputeSummary(int type, int numThreads, FILE* afp, FILE* rfp) {
#ifdef LATENCY
	ticks totalEnqueueTicks = 0, totalDequeueTicks = 0;
	ticks enqueuetickMin = enqueuetimestamp[0];
	ticks enqueuetickMax = enqueuetimestamp[0];
	ticks dequeuetickMin = dequeuetimestamp[0];
	ticks dequeuetickMax = dequeuetimestamp[0];
	ticks *numEnqueueTicks, *numDequeueTicks;
	numEnqueueTicks = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES);
	numDequeueTicks = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES);

	for (int i = 0; i < NUM_SAMPLES; i++) {
		numEnqueueTicks[i] = enqueuetimestamp[i];
		numDequeueTicks[i] = dequeuetimestamp[i];
		totalEnqueueTicks += numEnqueueTicks[i];
		totalDequeueTicks += numDequeueTicks[i];
	}

	SortTicks(numEnqueueTicks, numEnqueue, 0);
	SortTicks(numDequeueTicks, numDequeue, 0);

	enqueuetickMin = numEnqueueTicks[0];
	enqueuetickMax = numEnqueueTicks[numEnqueue - 1];

	dequeuetickMin = numDequeueTicks[0];
	dequeuetickMax = numDequeueTicks[numDequeue - 1];

	double tickEnqueueAverage = (totalEnqueueTicks / (NUM_SAMPLES));
	double tickDequeueAverage = (totalDequeueTicks / (NUM_SAMPLES));

	ticks enqueuetickmedian = 0, dequeuetickmedian = 0;

	if (NUM_SAMPLES % 2 == 0) {
		enqueuetickmedian = ((numEnqueueTicks[(numEnqueue / 2)] + numEnqueueTicks[(numEnqueue / 2) - 1]) / 2.0);
	} else {
		enqueuetickmedian = numEnqueueTicks[(numEnqueue / 2)];
	}

	if (NUM_SAMPLES % 2 == 0) {
		dequeuetickmedian = ((numDequeueTicks[((numDequeue) / 2)] + numDequeueTicks[((numDequeue) / 2) - 1]) / 2.0);
	} else {
		dequeuetickmedian = numDequeueTicks[((numDequeue) / 2)];
	}


	printf("%d %d %d %d %llu %llu %llu %llu %lf %lf %llu %llu\n", type, numThreads, numEnqueue, numDequeue, enqueuetickMin, dequeuetickMin, enqueuetickMax, dequeuetickMax, tickEnqueueAverage, tickDequeueAverage, enqueuetickmedian, dequeuetickmedian);
	fprintf(afp, "%d %d %d %d %llu %llu %llu %llu %lf %lf %llu %llu\n", type, numThreads, numEnqueue, numDequeue, enqueuetickMin, dequeuetickMin, enqueuetickMax, dequeuetickMax, tickEnqueueAverage, tickDequeueAverage, enqueuetickmedian, dequeuetickmedian);

	sortArray(enqueueFile, NUM_SAMPLES);
	sortArray(dequeueFile, NUM_SAMPLES);

	if(check(enqueueFile, dequeueFile)==false){
		printf("There was a mismatch in the array.\n");
	}
//	else{
//		for(int i=0; i<NUM_SAMPLES;i+1000){
//			fprintf(rfp, "%llu %llu\n", (numEnqueueTicks[i]), (numDequeueTicks[i]));
//		}
//	}

#endif

#ifdef THROUGHPUT
	fprintf(afp,"NumSamples NumThreads EnqueueThroughput DequeueThroughput\n");
        printf("NumSamples NumThreads EnqueueThroughput DequeueThroughput\n");
	printf("%d %d %f %f\n",NUM_SAMPLES, numThreads, enqueuethroughput,dequeuethroughput);
	fprintf(afp, "%d %d %f %f\n", NUM_SAMPLES, numThreads, enqueuethroughput, dequeuethroughput);
#endif

#ifdef LATENCY
	free(numEnqueueTicks);
	free(numDequeueTicks);
#endif
}

void ResetCounters(){
	numEnqueue = 0;
	numDequeue = 0;
	dequeuethroughput = 0;
	enqueuethroughput = 0;
	dequeue_ticks = 0;
	enqueue_ticks = 0;
}

int main(int argc, char **argv) {

	 int threadCount = 0;
	 int queueType;
	 int *threads = (int*)malloc(sizeof (int*));
	 char* fileName1;
	 char* fileName2;
	 if (argc != 5) {
		 printf("Usage: <QueueType 1-SQueue, 2-Wait-Free, 3-B-Queue> \nThreads-1,2,4,6,8,12,16,24,32,48,64 \nSummary file name: <name>\nRaw data file name: ");
		 exit(-1);
	 }
	 else {
	    char* arg = argv[1];
	    queueType = atoi(arg);

	    switch (queueType) {
	    	case 1:
	    		printf("Queue type: SQueue\n");
	    		break;
	    	case 2:
	    		printf("Queue type: Wait-Free\n");
	    		break;
	    	case 3:
	    		printf("Queue type: B-Queue\n");
	    		break;
	    	default:
	    		printf("Usage: <QueueType 1-SQueue, 2-Wait-Free, 3-B-Queue>, \nThreads-1,2,4,6,8,12,16,24,32,48,64 \nSummary file name: <name>\nRaw data file name: ");
	    		exit(-1);
	    		break;
	    }
	    char* str = argv[2];
	    char *thread = strtok(str, ",");
	    printf("Thread list: ");
	 	while(thread != NULL){
	 		threads[threadCount]=atoi(thread);
	 		threadCount++;
	 		printf("%s", thread);
	 		thread=strtok(NULL, ",");
	 	}
	 	printf("\n");
	 	fileName1 = argv[3];
		fileName2 = argv[4];
	 	printf("Number of samples: %d\n", NUM_SAMPLES);
	 	printf("Thread list count: %d\n", threadCount);
	 	printf("Output file: %s\n", fileName1);
		printf("Raw data file: %s\n", fileName2);
	 }

	 FILE *afp = fopen(fileName1, "a");
	 FILE *rfp = fopen(fileName2, "a");


	 cpu_set_t set;
	 CPU_ZERO(&set);
	 CPU_SET(0, &set);

	 pthread_setaffinity_np(pthread_self(), sizeof(set), &set);

#ifdef LATENCY
	fprintf(afp, "QueueType NumThreads EnqueueSamples DequeueSamples EnqueueMin DequeueMin EnqueueMax DequeueMax EnqueueAverage DequeueAverage EnqueueMedian DequeueMedian\n");
	printf("QueueType NumThreads EnqueueSamples DequeueSamples EnqueueMin DequeueMin EnqueueMax DequeueMax EnqueueAverage DequeueAverage EnqueueMedian DequeueMedian\n");
#endif

	 switch (queueType) {
		case 1:
			for (int k = 0; k < threadCount; k++) {
				InitQueue();
				ResetCounters();
				enqueuetimestamp = (ticks *) malloc(sizeof(ticks) * NUM_SAMPLES);
				dequeuetimestamp = (ticks *) malloc(sizeof(ticks) * NUM_SAMPLES);

				for (int i = 0; i < NUM_SAMPLES; i++) {
					enqueuetimestamp[i] = (ticks) 0;
					dequeuetimestamp[i] = (ticks) 0;
				}
				CUR_NUM_THREADS = (threads[k]) / 2;

				pthread_t *worker_threads = (pthread_t *) malloc(sizeof(pthread_t) * CUR_NUM_THREADS);
				pthread_t *enqueue_threads = (pthread_t *) malloc(sizeof(pthread_t) * CUR_NUM_THREADS);

				//Set number of threads that will call the barrier_wait to total of enqueue and dequeue threads
				pthread_barrier_init(&barrier, NULL, threads[k]);

				for (int i = 0; i < CUR_NUM_THREADS; i++) {
					pthread_create(&enqueue_threads[i], NULL, enqueue_handler, (void*) (unsigned long) (i));
					pthread_create(&worker_threads[i], NULL, worker_handler, (void*) (unsigned long) (i + CUR_NUM_THREADS));
				}

				for (int i = 0; i < CUR_NUM_THREADS; i++) {
					pthread_join(enqueue_threads[i], NULL);
					pthread_join(worker_threads[i], NULL);
				}

				ComputeSummary(queueType, CUR_NUM_THREADS, afp, rfp);

				free(enqueuetimestamp);
				free(dequeuetimestamp);
			}
			break;
		case 2:
			for (int k = 0; k < threadCount; k++) {
				ResetCounters();

				enqueuetimestamp = (ticks *) malloc(sizeof(ticks) * NUM_SAMPLES);
				dequeuetimestamp = (ticks *) malloc(sizeof(ticks) * NUM_SAMPLES);

				for (int i = 0; i < NUM_SAMPLES; i++) {
					enqueuetimestamp[i] = (ticks) 0;
					dequeuetimestamp[i] = (ticks) 0;
				}
				CUR_NUM_THREADS = (threads[k]) / 2;

				KoganPetrankQueueCHP<int> queue;

				pthread_t *worker_threads = (pthread_t *) malloc(sizeof(pthread_t) * CUR_NUM_THREADS);
				pthread_t *enqueue_threads = (pthread_t *) malloc(sizeof(pthread_t) * CUR_NUM_THREADS);
				pthread_barrier_init(&barrier, NULL, threads[k]);

				for (int i = 0; i < CUR_NUM_THREADS; i++) {
					struct arg_queue* refQueue = (struct arg_queue*) malloc(sizeof(struct arg_queue));
					refQueue->queue = &queue;
					refQueue->i = i;
					pthread_create(&enqueue_threads[i], NULL, push_enq,((void*) refQueue));
				}

				for (int i = CUR_NUM_THREADS; i < (CUR_NUM_THREADS * 2); i++) {
					struct arg_queue* refQueue = (struct arg_queue*) malloc(sizeof(struct arg_queue));
					refQueue->queue = &queue;
					refQueue->i = i;
					pthread_create(&worker_threads[i - CUR_NUM_THREADS], NULL, pop_deq,((void*) refQueue));
				}

				for (int i = 0; i < CUR_NUM_THREADS; i++) {
					pthread_join(enqueue_threads[i], NULL);
					pthread_join(worker_threads[i], NULL);
				}

				ComputeSummary(queueType, CUR_NUM_THREADS, afp, rfp);

				free(enqueuetimestamp);
				free(dequeuetimestamp);
			}
			break;
		case 3:
			for(int k = 0; k < threadCount; k++) {
				ResetCounters();

				enqueuetimestamp = (ticks *) malloc(sizeof(ticks) * NUM_SAMPLES);
				dequeuetimestamp = (ticks *) malloc(sizeof(ticks) * NUM_SAMPLES);

				for (int i = 0; i < NUM_SAMPLES; i++) {
					enqueuetimestamp[i] = (ticks) 0;
					dequeuetimestamp[i] = (ticks) 0;
				}
				CUR_NUM_THREADS = (threads[k]) / 2;

				q = (struct queue_t*)malloc(sizeof(struct queue_t));
				queue_init(q);

				pthread_t *b_worker_threads = (pthread_t *) malloc(sizeof(pthread_t) * CUR_NUM_THREADS);
				pthread_t *b_enqueue_threads = (pthread_t *) malloc(sizeof(pthread_t) * CUR_NUM_THREADS);

				pthread_barrier_init(&barrier, NULL, CUR_NUM_THREADS * 2);

				for (int i = 0; i < CUR_NUM_THREADS; i++) {
					struct threaddata * td = (struct threaddata * ) malloc(sizeof(struct threaddata));
					td->my_cpu = i;
					td->startIndex = (i * (NUM_SAMPLES/CUR_NUM_THREADS)) + 1;
					pthread_create(&b_enqueue_threads[i], NULL, b_enqueue_handler,(void*)td);
					pthread_create(&b_worker_threads[i], NULL, b_worker_handler, (void*)td);
				}

				for (int i = 0; i < CUR_NUM_THREADS; i++) {
					pthread_join(b_enqueue_threads[i], NULL);
					pthread_join(b_worker_threads[i], NULL);
				}

				ComputeSummary(queueType, CUR_NUM_THREADS, afp, rfp);

				free(enqueuetimestamp);
				free(dequeuetimestamp);
			}
			break;
	}
	return 0;
}
