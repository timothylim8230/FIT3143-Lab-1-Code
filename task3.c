#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <omp.h>
#include <time.h>
#include <math.h>

#define OUTPUT_FILE "primes_openmp.txt"

// How many candidates each parallel block covers. One byte per candidate, so
// this is also the block's size in bytes - kept small enough to sit in L1/L2
// cache while a thread works on it. Tunable: see the block-size experiment.
#define BLOCK_SIZE 32768



static double now_seconds(void)  
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + (ts.tv_nsec * 1e-9);
}



int main(int argc, char *argv[])
{
    //1. PARSE + VALIDATE   argc == 3,   n>= 2 ,  numThreads >= 1
    if (argc != 3){
        return 1;
    }

    int n = atoi(argv[1]);
    int numThreads = atoi(argv[2]);

    if (n < 2) {
        return 1;
    }

    if (numThreads < 1){
        return 1;
    }

    omp_set_num_threads(numThreads);

    // Everything from here to the end of the collection pass is timed, so
    // totalTime covers the serial work as well as the parallel phase.
    double startTime = now_seconds();

    bool *isPrime = malloc((size_t) n * sizeof(bool));
    if (isPrime == NULL){
        return 1;
    }

    int sqrtN = sqrt((double)n) + 1;

    if (sqrtN > n - 1) {
        sqrtN = n - 1;
    }

    for (int i = 0; i < n; i++) {
        isPrime[i] = true;
    }

    for (int i = 2; i <= sqrtN; i++) {
        if (isPrime[i]) {
            for (int j = i * i; j <= sqrtN; j += i) {
                isPrime[j] = false;
            }
        }
    }

    // Count the primes the serial sieve just settled, i.e. those in [2, sqrtN].
    // The parallel phase below only counts what it finds above sqrtN.
    int totalPrimes = 0;
    for (int i = 2; i <= sqrtN; i++) {
        if (isPrime[i]) {
            totalPrimes++;
        }
    }

    // ---- step 2 (parallel): block decomposition ----------------------------
    // The remaining range [first, n) is cut into BLOCK_SIZE-wide contiguous
    // blocks, and the parallel for hands blocks out to threads. There are far
    // more blocks than threads on purpose: that surplus is what the schedule
    // clause needs in order to balance the load, because a thread that finishes
    // a block early can take the next one instead of idling.
    //
    // Each block is crossed off and counted entirely by one thread, and no
    // write ever leaves [sp, ep), so blocks are disjoint and the array needs no
    // lock - the same argument task2 makes about its slices. totalPrimes is the
    // one shared value, and reduction(+:) gives every thread a private copy
    // that is summed at the barrier. That replaces task2's mutex.
    int first     = sqrtN + 1;
    int numBlocks = (n - first + BLOCK_SIZE - 1) / BLOCK_SIZE;   // divide, rounding up

    // Timed separately from totalTime: this is the only part more threads can
    // speed up, so it is what the speedup graphs are computed from.
    double searchTime = now_seconds();

    #pragma omp parallel for schedule(dynamic) reduction(+:totalPrimes)
    for (int b = 0; b < numBlocks; b++) {

        int sp = first + b * BLOCK_SIZE;    // start point of this block
        int ep = sp + BLOCK_SIZE;           // end point, one past the last
        if (ep > n) {
            ep = n;             // the last block stops at n, not past it
        }

        // Cross off the multiples of every small prime that land in this block.
        for (int i = 2; i <= sqrtN; i++) {

            if (!isPrime[i]) {
                continue;       // i is composite, its multiples are already gone
            }

            // First multiple of i at or above sp. Never start below i*i: any
            // smaller multiple of i has a smaller prime factor and was crossed
            // off by an earlier value of i.
            int j = i * i;
            if (j < sp) {
                j = (sp / i) * i;   // round sp down to a multiple of i
                if (j < sp) {
                    j += i;         // then step up to land inside the block
                }
            }

            for (; j < ep; j += i) {
                isPrime[j] = false;
            }
        }

        // Count the survivors. Safe to do here: this block is now fully sieved
        // and no other thread ever writes into [sp, ep).
        for (int k = sp; k < ep; k++) {
            if (isPrime[k]) {
                totalPrimes++;
            }
        }
    }


    searchTime = now_seconds() - searchTime;

    // ---- collect the primes (serial) ---------------------------------------
    // totalPrimes is already exact: the serial sieve counted [2, sqrtN] and the
    // blocks counted [sqrtN+1, n) between them, with no gap and no overlap, so
    // the two together cover [2, n) once.
    //
    // This pass stays serial on purpose. Walking the array from low to high
    // emits the primes in ascending order for free, so there is nothing to sort
    // or merge. Splitting it across threads would mean each thread needed to
    // know how many primes came before its slice, which is a prefix sum - more
    // synchronisation than the pass itself costs.
    int *primes = malloc((size_t) totalPrimes * sizeof(int));
    if (totalPrimes > 0 && primes == NULL) {   // malloc(0) may return NULL legally
        free(isPrime);
        return 1;
    }

    int count = 0;
    for (int i = 2; i < n; i++) {
        if (isPrime[i]) {
            primes[count++] = i;
        }
    }

    double totalTime = now_seconds() - startTime;

    // ---- output ------------------------------------------------------------
    if (n < 100) {

        // (a) small n: straight to the screen
        for (int i = 0; i < count; i++) {
            printf("%d ", primes[i]);
        }
        printf("\n");

    } else {

        // (b) large n: one prime per line in a text file
        FILE *out = fopen(OUTPUT_FILE, "w");
        if (out == NULL) {
            printf("Could not open %s for writing.\n", OUTPUT_FILE);
            free(primes);
            free(isPrime);
            return 1;
        }
        for (int i = 0; i < count; i++) {
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


