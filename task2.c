// FIT3143 Lab 1 - Task 2
// Parallel prime search using POSIX threads.
// Partitioning scheme: static block decomposition of the candidate range.
//
// Build: gcc -O2 -Wall -static -o task2.exe task2.c -lm -pthread
// Run:   ./task2.exe <n> <numThreads>
//
// -static matters on Windows: if another toolchain (Anaconda ships one) has
// its own libwinpthread-1.dll earlier on PATH, a dynamically linked build
// loads the wrong DLL and dies at startup with 0xC0000139.

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <pthread.h>
#include <time.h>

#define OUTPUT_FILE  "primes_parallel.txt"
#define MAX_THREADS  256


// ---- shared state -------------------------------------------------------
// g_isPrime is shared, but each thread writes only to its own [lo, hi)
// slice, so the writes never overlap and no mutex is required here.
static bool *g_isPrime;


// One of these per thread. Each thread gets the address of its OWN element
// of the array (see slides 21-23): passing &i from the creation loop would
// race, because i keeps changing while the threads are starting up.
typedef struct {
    int id;
    int lo;      // first candidate this thread owns (inclusive)
    int hi;      // one past the last candidate  (exclusive)
    int count;   // primes found; written by the thread, read by main after join
} ThreadArg;


// Trial division, matching the approach used in the serial task1.
// d * d <= c is used instead of sqrt() so there is no floating point
// rounding risk on perfect squares (e.g. sqrt(25) coming back as 4.9999).
static bool is_prime(int c) {
    if (c < 2)      return false;
    if (c % 2 == 0) return c == 2;
    for (long long d = 3; d * d <= c; d += 2) {
        if (c % d == 0) return false;
    }
    return true;
}

static pthread_mutex_t g_Mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_count = 0;

static void *worker(void *arg) {
    ThreadArg *a = (ThreadArg *) arg;
    int myCount = 0;

    for (int c = a->lo; c < a->hi; c++) {
        g_isPrime[c] = is_prime(c);       // disjoint slice - no lock needed
        if (g_isPrime[c]) myCount++;
    }

    // Own struct element, read by main only after the join - no lock needed.
    a->count = myCount;

    // The critical section holds nothing but the shared accumulate. Printing
    // in here would serialise the threads on console I/O and would land that
    // latency inside the timed region, so main reports the per-thread lines
    // after joining instead.
    pthread_mutex_lock(&g_Mutex);
    g_count += myCount;
    pthread_mutex_unlock(&g_Mutex);

    return NULL;
}


// wall-clock seconds. NOT clock(): clock() returns CPU time summed over all
// threads, which would make the parallel run look slower the more threads
// you use.
static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}


// atoi() cannot tell "0" from a non-numeric argument, so parse strictly and
// let the caller report the bad input.
static bool parse_int(const char *text, int *out) {
    char *end;
    long value = strtol(text, &end, 10);

    if (end == text || *end != '\0' || value < INT_MIN || value > INT_MAX) {
        return false;
    }
    *out = (int) value;
    return true;
}


int main(int argc, char **argv) {

    // Everything the cleanup path frees is declared up front and NULL until
    // it owns memory, so any failure below can jump straight to `cleanup`.
    pthread_t *tid  = NULL;
    ThreadArg *args = NULL;
    int *primes     = NULL;
    int created     = 0;
    int retval      = 0;
    int n, numThreads;

    if (argc != 3) {
        printf("Usage: %s <n> <numThreads>\n", argv[0]);
        return 1;
    }

    if (!parse_int(argv[1], &n) || !parse_int(argv[2], &numThreads)) {
        printf("Both arguments must be integers.\n");
        return 1;
    }

    if (n < 2 || numThreads < 1 || numThreads > MAX_THREADS) {
        printf("Need n >= 2 and 1 <= numThreads <= %d.\n", MAX_THREADS);
        return 1;
    }

    g_isPrime = malloc((size_t) n * sizeof(bool));
    tid       = malloc((size_t) numThreads * sizeof(pthread_t));
    args      = malloc((size_t) numThreads * sizeof(ThreadArg));
    if (g_isPrime == NULL || tid == NULL || args == NULL) {
        printf("Could not allocate working arrays for n = %d.\n", n);
        retval = 1;
        goto cleanup;
    }

    // ---- static block decomposition ------------------------------------
    // Candidates 2 .. n-1 are split into numThreads contiguous blocks.
    // The remainder is spread one-per-thread over the first few threads so
    // block sizes differ by at most 1.
    int total = n - 2;
    int base  = total / numThreads;
    int rem   = total % numThreads;

    int lo = 2;
    for (int t = 0; t < numThreads; t++) {
        int len = base + (t < rem ? 1 : 0);
        args[t].id = t;
        args[t].lo = lo;
        args[t].hi = lo + len;
        lo += len;
    }

    // ---- parallel search ------------------------------------------------
    double t0 = now_seconds();

    for (int t = 0; t < numThreads; t++) {
        if (pthread_create(&tid[t], NULL, worker, &args[t]) != 0) {
            printf("pthread_create failed for thread %d\n", t);
            retval = 1;
            break;
        }
        created++;
    }

    // Join whatever actually started, including on the failure path above:
    // returning while threads are still writing to g_isPrime would pull the
    // array out from under them.
    for (int t = 0; t < created; t++) {
        pthread_join(tid[t], NULL);
    }

    if (retval != 0) {
        goto cleanup;
    }

    double searchTime = now_seconds() - t0;

    // ---- collect results ------------------------------------------------
    // Sweeping the shared array from low to high yields the primes already
    // in ascending order, so no sorting or merging step is needed.
    // g_count is final now that every thread has been joined, so it sizes the
    // output array directly and saves a separate counting pass.
    int count = g_count;

    if (count > 0) {
        primes = malloc((size_t) count * sizeof(int));
        if (primes == NULL) {
            printf("Could not allocate primes array.\n");
            retval = 1;
            goto cleanup;
        }
    }

    int index = 0;
    for (int c = 2; c < n; c++) {
        if (g_isPrime[c]) {
            primes[index++] = c;
        }
    }

    double totalTime = now_seconds() - t0;

    // ---- per-thread breakdown (after both timers have been read) ---------
    for (int t = 0; t < numThreads; t++) {
        printf("Thread %d: %d primes in [%d, %d)\n",
               args[t].id, args[t].count, args[t].lo, args[t].hi);
    }

    // ---- output ---------------------------------------------------------
    if (n < 100) {

        // (a) small n: standard output
        printf("Primes below %d:\n", n);
        for (int i = 0; i < count; i++) {
            printf("%d ", primes[i]);
        }
        printf("\n");

    } else {

        // (b) large n: text file
        FILE *out = fopen(OUTPUT_FILE, "w");
        if (out == NULL) {
            printf("Could not open %s for writing.\n", OUTPUT_FILE);
            retval = 1;
            goto cleanup;
        }
        for (int i = 0; i < count; i++) {
            fprintf(out, "%d\n", primes[i]);
        }
        fclose(out);
        printf("Wrote %d primes to %s\n", count, OUTPUT_FILE);
    }

    // one-line summary for tabulating results
    printf("n=%d threads=%d primes=%d search=%.4f s total=%.4f s\n",
           n, numThreads, count, searchTime, totalTime);

cleanup:
    free(primes);
    primes = NULL;
    free(args);
    args = NULL;
    free(tid);
    tid = NULL;
    free(g_isPrime);
    g_isPrime = NULL;
    pthread_mutex_destroy(&g_Mutex);

    return retval;
}
