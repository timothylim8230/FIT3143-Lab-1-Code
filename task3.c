/**
// Build: gcc -O2 -Wall -fopenmp -o task3.exe task3.c -lm
// Run:     ./task3.exe <n> <numThreads>
 * Timothy Lim  33111472          tlim0034@student.monash.edu
 * Scott Nguyen 33879095 sngu0065@student.monash.edu
 */



#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <omp.h>
#include <time.h>
#include <math.h>

#define OUTPUT_FILE "primes_openmp.txt"

static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + (ts.tv_nsec * 1e-9);
}

int main(int argc, char *argv[])
{

    if (argc != 3)
    {
        return 1;
    }

    int n = atoi(argv[1]);
    int numThreads = atoi(argv[2]);

    if (n < 2)
    {
        return 1;
    }

    if (numThreads < 1)
    {
        return 1;
    }

    omp_set_num_threads(numThreads);

    // Everything from here to the end of the collection pass is timed, so
    // totalTime covers the serial work as well as the parallel phase.
    double startTime = now_seconds();

    bool *isPrime = malloc((size_t)n * sizeof(bool));
    if (isPrime == NULL)
    {
        return 1;
    }

    int sqrtN = sqrt((double)n) + 1;

    if (sqrtN > n - 1)
    {
        sqrtN = n - 1;
    }

    for (int i = 0; i < n; i++)
    {
        isPrime[i] = true;
    }

    for (int i = 2; i <= sqrtN; i++)
    {
        if (isPrime[i])
        {
            for (int j = i * i; j <= sqrtN; j += i)
            {
                isPrime[j] = false;
            }
        }
    }

    // Count the primes the serial sieve just settled, i.e. those in [2, sqrtN].
    // The parallel phase below only counts what it finds above sqrtN.
    int totalPrimes = 0;
    for (int i = 2; i <= sqrtN; i++)
    {
        if (isPrime[i])
        {
            totalPrimes++;
        }
    }

    // split the range above sqrt(n) into one equal slice per thread.
    // same block decomposition as task2, so comparing the two is fair.
    // each thread only writes inside its own slice, so no mutex is needed.
    // reduction(+:) gives each thread its own copy of totalPrimes and adds
    // them up at the end - this is what replaces task2's mutex.
    

    int first = sqrtN + 1;
    int total = n - first;
    if (total < 0)
    {
        total = 0;
    }

    // Timed separately from totalTime: this is the only part more threads can
    // speed up, so it is what the speedup graphs are computed from.
    double searchTime = now_seconds();
    // omp parallel, not parallel for: each thread works out its own slice
    // from its thread number, the same way task2 does from the thread id
#pragma omp parallel reduction(+ : totalPrimes)


#pragma omp parallel reduction(+ : totalPrimes)
    {
        int myRank = omp_get_thread_num();
        int numT = omp_get_num_threads();

        // Same slice arithmetic as task2. The long long cast keeps
        // total * myRank from overflowing int at large n.
        int sp = first + (int)((long long)total * myRank / numT);       // inclusive
        int ep = first + (int)((long long)total * (myRank + 1) / numT); // exclusive

        // Cross off the multiples of every base prime that land in this slice.
        for (int i = 2; i <= sqrtN; i++)
        {

            if (!isPrime[i])
            {
                continue; // i is composite, its multiples are already gone
            }

            // First multiple of i at or above sp. Never start below i*i: any
            // smaller multiple of i has a smaller prime factor and was crossed
            // off by an earlier value of i.
            int j = i * i;
            if (j < sp)
            {
                j = (sp / i) * i; // round sp down to a multiple of i
                if (j < sp)
                {
                    j += i; // then step up to land inside the slice
                }
            }

            for (; j < ep; j += i)
            {
                isPrime[j] = false;
            }
        }

        // Count the survivors. Safe here: this slice is now fully sieved and no
        // other thread ever writes into [sp, ep).
        for (int k = sp; k < ep; k++)
        {
            if (isPrime[k])
            {
                totalPrimes++;
            }
        }
    }

    searchTime = now_seconds() - searchTime;


    int *primes = malloc((size_t)totalPrimes * sizeof(int));
    if (totalPrimes > 0 && primes == NULL)
    { // malloc(0) may return NULL legally
        free(isPrime);
        return 1;
    }

    int count = 0;
    for (int i = 2; i < n; i++)
    {
        if (isPrime[i])
        {
            primes[count++] = i;
        }
    }

    double totalTime = now_seconds() - startTime;

    if (n < 100)
    {
        for (int i = 0; i < count; i++)
        {
            printf("%d ", primes[i]);
        }
        printf("\n");
    }
    else
    {

        FILE *out = fopen(OUTPUT_FILE, "w");
        if (out == NULL)
        {
            printf("Could not open %s for writing.\n", OUTPUT_FILE);
            free(primes);
            free(isPrime);
            return 1;
        }
        for (int i = 0; i < count; i++)
        {
            fprintf(out, "%d\n", primes[i]);
        }
        fclose(out);
    }

    // Timings last, so writing the output above cannot affect them. One line
    // per run, ready to paste into the speedup tables.
    printf("n=%d threads=%d primes=%d search=%.4f s total=%.4f s\n",
           n, numThreads, count, searchTime, totalTime);

    free(primes);
    free(isPrime);

    return 0;
}
