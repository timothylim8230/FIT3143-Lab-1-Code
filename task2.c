// Build: gcc task2.c -lm -pthread
// Run:   ./a.out <n> <numThreads>

// Uses segmented sieve

/**
 * Timothy Lim           tlim0034@student.monash.edu
 * Scott Nguyen 33879095 sngu0065@student.monash.edu
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <pthread.h>

#define MAX_THREADS 64

bool *isPrime;      
int   n;            
int   sqrtN;        


// totalPrimes will be a resource accessed via mutex as it is shared
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int totalPrimes = 0;   

typedef struct {
    int    id;
    int    start;   // first number in given range (inclusive)
    int    end;     // the last number in given range (exclusive)
    int    count;   // primes this thread found
    double time;    // seconds this thread took, used to check the load balance
} ThreadData;


// Wall-clock time in seconds. Not clock(), which adds up the CPU time of every
// thread and so makes the parallel run look slower the more threads you use.
double getTime()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + (ts.tv_nsec * 1e-9);
}


// Run by every thread
void *findPrimes(void *arg)
{

    // get thread data
    ThreadData *data = (ThreadData*)arg; 
    int startOfThreadRange = (*data).start;
    int endOfThreadRange = (*data).end;

    // start timer that tracks how long thread takes to compute primes in specified range
    double startTime = getTime(); 

    // initialise count of primes in this thread
    int count = 0;
   
    for (int i = 2; i <= sqrtN; i++) { // loop through base primes (up to sqrt(n))

        if (!isPrime[i]) {
            continue;       // i is composite, its multiples are already crossed off
        }

        // start checking primes from i^2 
        int j = i * i;
        if (j < startOfThreadRange) {
            j = (startOfThreadRange / i) * i;      // round start of range to a multiple of i if higher than i^2
            if (j < startOfThreadRange) {
                j += i;                     // goes to next multiple if still outside range
            }
        }

        // cross off all multiples of i in thread range
        while (j < endOfThreadRange) {
            isPrime[j] = false;
            j += i;
        }
    }

    // count all primes found in this thread
    for (int k = startOfThreadRange; k < endOfThreadRange; k++) {
        if (isPrime[k]) {
            count ++;
        }
    }
    

    // update thread stats
    (*data).count = count;
    (*data).time  = getTime() - startTime;

    // totalPrimes is shared, so use mutex to update it safely
    pthread_mutex_lock(&mutex);
    totalPrimes += count;
    pthread_mutex_unlock(&mutex);

    return NULL;
}


int main(int argc, char **argv)
{
    int numThreads;
    double startTime, searchTime, totalTime;

    if (argc != 3) {
        printf("Must be run using ./a.out <n> <numThreads>");
        return 1;
    }


    n = atoi(argv[1]);
    numThreads = atoi(argv[2]);

    if (n < 2 || numThreads < 1 || numThreads > MAX_THREADS) {
        printf("Need n >= 2 and 1 <= numThreads <= %d.\n", MAX_THREADS);
        return 1;
    }

    pthread_t  tid[MAX_THREADS];
    ThreadData data[MAX_THREADS];

    startTime = getTime();

    isPrime = malloc(n * sizeof(bool));
    if (isPrime == NULL) {
        printf("Could not allocate the array for n = %d.\n", n);
        return 1;
    }

    // initialise primes array
    for (int i = 0; i < n; i++) {
        isPrime[i] = true;
    }
    
    sqrtN = sqrt(n);

    // mark off all base primes (below sqrt(n))
    for (int i = 2; i <= sqrtN; i++) {
        if (isPrime[i]) {
            for (int j = i * i; j <= sqrtN; j += i) {
                isPrime[j] = false;
            }
        }
    }

    // add count of base primes to total
    for (int i = 2; i <= sqrtN; i++) {
        if (isPrime[i]) {
            totalPrimes++;
        }
    }

   
    int first = sqrtN + 1;
    int total = n - first;
    if (total < 0) {
        total = 0;
    }

    // initialise thread data
    for (int i = 0; i < numThreads; i++) {
        data[i].id    = i;
        data[i].start = first + (int)((long long)total * i / numThreads);
        data[i].end   = first + (int)((long long)total * (i + 1) / numThreads);
        data[i].count = 0;
        data[i].time  = 0.0;
    }

    
    searchTime = getTime();

    for (int i = 0; i < numThreads; i++) {
        pthread_create(&tid[i], NULL, findPrimes, &data[i]);
    }
    for (int i = 0; i < numThreads; i++) {
        pthread_join(tid[i], NULL);
    }

    searchTime = getTime() - searchTime;

    int *primes = malloc(totalPrimes * sizeof(int));
    if (primes == NULL) {
        printf("Could not allocate the primes list.\n");
        return 1;
    }

    int count = 0;
    for (int i = 2; i < n; i++) {
        if (isPrime[i]) {
            primes[count++] = i;
        }
    }

    totalTime = getTime() - startTime;


    if (n < 100) {

        // small n: print to standard output
        printf("Primes below %d:\n", n);
        for (int i = 0; i < count; i++) {
            printf("%d ", primes[i]);
        }
        printf("\n");

    } else {

        // large n: write to a text file
        FILE *out = fopen("primes2.txt", "w");
        if (out == NULL) {
            printf("Could not open primes2.txt for writing.\n");
            free(isPrime);
            return 1;
        }
        for (int i = 0; i < count; i++) {
            fprintf(out, "%d\n", primes[i]);
        }
        fclose(out);
        printf("Primes below n written  to primes2.txt\n");
    }

    printf("n=%d threads=%d primes=%d search=%.4f s total=%.4f s\n",
           n, numThreads, count, searchTime, totalTime);

    free(primes);
    free(isPrime);

    return 0;
}
