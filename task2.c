//---------------------------------------------------------------------------------------------------------------------
// task2.c
// FIT3143 Lab 1 - Task 2
//
// Parallel prime search using POSIX threads.
//
// Partitioning scheme: parallel segmented Sieve of Eratosthenes.
//
//   Stage 1 (serial):   sieve the "base primes" up to sqrt(n). For n = 100,000,000
//                       that is every prime below 10,000 - a few thousand values,
//                       so this stage is negligible but it IS the serial fraction
//                       that bounds the achievable speed-up (Amdahl's law).
//
//   Stage 2 (parallel): split the candidate range [2, n) into numThreads
//                       contiguous blocks of near-equal size. Each thread crosses
//                       off the multiples of every base prime that fall inside its
//                       OWN block, then counts the survivors in that block.
//
// Why this partitioning balances the workload:
//   The cost of sieving a block is proportional to (block size) x sum(1/p) over the
//   base primes p. That sum does not depend on WHERE the block sits in the range, so
//   equal-sized blocks carry near-equal work. Contrast this with splitting a trial
//   division search, where testing c costs O(sqrt(c)) and the thread holding the
//   highest block does far more work than the thread holding the lowest.
//   Each thread also touches only its own block, which is cache friendly.
//
// Build: gcc -O2 -Wall -static -o task2.exe task2.c -lm -pthread
// Run:   ./task2.exe <n> <numThreads>
//
// -static matters on Windows: if another toolchain (Anaconda ships one) has its own
// libwinpthread-1.dll earlier on PATH, a dynamically linked build loads the wrong
// DLL and dies at startup with 0xC0000139.
//---------------------------------------------------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>

#define OUTPUT_FILE  "primes_parallel.txt"
#define MAX_THREADS  256


// ---- shared state ---------------------------------------------------------------------------------------------------
// gIsComposite is shared, but each thread writes only to its own [startVal, endVal)
// block, so no two threads ever touch the same byte and no mutex is needed for it.
// gBasePrimes is written before any thread starts and is read-only afterwards.
static char *gIsComposite = NULL;   // 1 = composite, 0 = prime (calloc gives us all 0)
static int  *gBasePrimes  = NULL;   // primes up to sqrt(n)
static int   gNumBase     = 0;

// gPrimeCount is a genuinely shared scalar, so it IS guarded (see slide 28).
static pthread_mutex_t gMutex;
static int gPrimeCount = 0;


// One of these per thread. Each thread receives the address of its OWN element of
// the array (slides 21-23): passing &i from the creation loop would race, because i
// keeps changing while the threads are still starting up.
typedef struct {
    int    threadNum;
    int    startVal;    // first candidate this thread owns (inclusive)
    int    endVal;      // one past the last candidate     (exclusive)
    int    primeCount;  // primes found in this block
    double timeTaken;   // wall time this thread spent, used to show load balance
} ThreadArg;


// Returns the current monotonic wall-clock time in seconds.
// NOT clock(): clock() returns CPU time summed over all threads, which would make
// the parallel run look slower the more threads you use.
double GetTimeSeconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + (ts.tv_nsec * 1e-9);
}


// Stage 1. Plain serial sieve over [2, limit] to produce the base primes.
// Returns the array (caller frees) and writes the count to *pOutCount.
int* ComputeBasePrimes(int limit, int *pOutCount)
{
    int i, j, count = 0;

    *pOutCount = 0;
    if (limit < 2) {
        return NULL;
    }

    char *pMark = (char*)calloc((size_t)limit + 1, sizeof(char));
    if (pMark == NULL) {
        return NULL;
    }

    for (i = 2; (long long)i * i <= limit; i++) {
        if (!pMark[i]) {
            for (j = i * i; j <= limit; j += i) {
                pMark[j] = 1;
            }
        }
    }

    for (i = 2; i <= limit; i++) {
        if (!pMark[i]) {
            count++;
        }
    }

    int *pPrimes = (int*)malloc((size_t)count * sizeof(int));
    if (pPrimes == NULL) {
        free(pMark);
        return NULL;
    }

    int index = 0;
    for (i = 2; i <= limit; i++) {
        if (!pMark[i]) {
            pPrimes[index++] = i;
        }
    }

    free(pMark);
    *pOutCount = count;
    return pPrimes;
}


// Stage 2. Cross off the multiples of every base prime inside this thread's block,
// then count what survived.
void *ThreadFunc(void *pArg)
{
    ThreadArg *pThreadArg = (ThreadArg*)pArg;
    int startVal = pThreadArg->startVal;
    int endVal   = pThreadArg->endVal;
    int i, myCount = 0;
    double start = GetTimeSeconds();

    for (i = 0; i < gNumBase; i++) {
        long long p = gBasePrimes[i];
        long long firstMultiple = p * p;

        // p*p is where marking may begin: every smaller multiple of p already has a
        // smaller prime factor and was handled by an earlier base prime. If the block
        // starts above p*p, round startVal up to the next multiple of p instead.
        if (firstMultiple < startVal) {
            firstMultiple = ((startVal + p - 1) / p) * p;
        }

        for (long long j = firstMultiple; j < endVal; j += p) {
            gIsComposite[j] = 1;        // disjoint block - no lock needed
        }
    }

    for (i = startVal; i < endVal; i++) {
        if (!gIsComposite[i]) {
            myCount++;
        }
    }

    pThreadArg->primeCount = myCount;
    pThreadArg->timeTaken  = GetTimeSeconds() - start;

    // Shared scalar - lock required. Nothing else goes in here: doing I/O while
    // holding the mutex would serialise the threads on the console and fold that
    // latency into the measured parallel time.
    pthread_mutex_lock(&gMutex);
    gPrimeCount += myCount;
    pthread_mutex_unlock(&gMutex);

    return NULL;
}


// atoi() cannot tell "0" from a non-numeric argument, so parse strictly.
int ParseInt(const char *pText, int *pOut)
{
    char *pEnd;
    long value = strtol(pText, &pEnd, 10);

    if (pEnd == pText || *pEnd != '\0' || value < 1 || value > 2000000000L) {
        return 0;
    }
    *pOut = (int)value;
    return 1;
}


int main(int argc, char **argv)
{
    pthread_t *pTid    = NULL;
    ThreadArg *pArgs   = NULL;
    int       *pPrimes = NULL;
    int i, n, numThreads;
    int created = 0, retval = 0;
    int mutexReady = 0;
    double tStart, tBase, tSearch, tTotal;

    if (argc != 3) {
        printf("Usage: %s <n> <numThreads>\n", argv[0]);
        return 1;
    }

    if (!ParseInt(argv[1], &n) || !ParseInt(argv[2], &numThreads)) {
        printf("Both arguments must be positive integers.\n");
        return 1;
    }

    if (n < 2 || numThreads < 1 || numThreads > MAX_THREADS) {
        printf("Need n >= 2 and 1 <= numThreads <= %d.\n", MAX_THREADS);
        return 1;
    }

    tStart = GetTimeSeconds();

    // ---- stage 1: base primes up to sqrt(n-1), serial ---------------------------------------------------------------
    // Nudge the sqrt() result rather than trusting the floating point value on a
    // perfect square (sqrt(25) can come back as 4.9999...).
    int limit = (int)sqrt((double)(n - 1));
    while ((long long)(limit + 1) * (limit + 1) <= (long long)(n - 1)) {
        limit++;
    }
    while ((long long)limit * limit > (long long)(n - 1)) {
        limit--;
    }

    gBasePrimes = ComputeBasePrimes(limit, &gNumBase);
    if (limit >= 2 && gBasePrimes == NULL) {
        printf("Could not compute base primes.\n");
        retval = 1;
        goto cleanup;
    }
    tBase = GetTimeSeconds() - tStart;

    // ---- allocate ---------------------------------------------------------------------------------------------------
    // calloc, not malloc + a fill loop: zeroed pages come straight from the OS, so
    // "nothing is composite yet" costs nothing.
    gIsComposite = (char*)calloc((size_t)n, sizeof(char));
    pTid         = (pthread_t*)malloc((size_t)numThreads * sizeof(pthread_t));
    pArgs        = (ThreadArg*)malloc((size_t)numThreads * sizeof(ThreadArg));
    if (gIsComposite == NULL || pTid == NULL || pArgs == NULL) {
        printf("Could not allocate working arrays for n = %d.\n", n);
        retval = 1;
        goto cleanup;
    }

    if (pthread_mutex_init(&gMutex, NULL) != 0) {
        printf("Could not initialise mutex.\n");
        retval = 1;
        goto cleanup;
    }
    mutexReady = 1;

    // ---- partition: contiguous blocks of near-equal size -------------------------------------------------------------
    // The remainder is spread one-per-thread over the first few threads, so block
    // sizes differ by at most 1 candidate.
    int total    = n - 2;
    int base     = total / numThreads;
    int rem      = total % numThreads;
    int startVal = 2;

    for (i = 0; i < numThreads; i++) {
        int len = base + (i < rem ? 1 : 0);
        pArgs[i].threadNum  = i;
        pArgs[i].startVal   = startVal;
        pArgs[i].endVal     = startVal + len;
        pArgs[i].primeCount = 0;
        pArgs[i].timeTaken  = 0.0;
        startVal += len;
    }

    // ---- parallel search: fork / join --------------------------------------------------------------------------------
    tSearch = GetTimeSeconds();

    // Fork
    for (i = 0; i < numThreads; i++) {
        if (pthread_create(&pTid[i], NULL, ThreadFunc, &pArgs[i]) != 0) {
            printf("pthread_create failed for thread %d\n", i);
            retval = 1;
            break;
        }
        created++;
    }

    // Join. This also runs on the failure path above: returning while threads are
    // still writing to gIsComposite would pull the array out from under them.
    for (i = 0; i < created; i++) {
        pthread_join(pTid[i], NULL);
    }

    if (retval != 0) {
        goto cleanup;
    }

    tSearch = GetTimeSeconds() - tSearch;

    // ---- collect results ---------------------------------------------------------------------------------------------
    // Sweeping the shared array from low to high yields the primes already in
    // ascending order, so no sorting or merging step is needed. gPrimeCount is final
    // now that every thread has been joined, so it sizes the output array directly.
    if (gPrimeCount > 0) {
        pPrimes = (int*)malloc((size_t)gPrimeCount * sizeof(int));
        if (pPrimes == NULL) {
            printf("Could not allocate primes array.\n");
            retval = 1;
            goto cleanup;
        }
    }

    int index = 0;
    for (i = 2; i < n; i++) {
        if (!gIsComposite[i]) {
            pPrimes[index++] = i;
        }
    }

    tTotal = GetTimeSeconds() - tStart;

    // ---- output -------------------------------------------------------------------------------------------------------
    if (n < 100) {

        // (a) small n: standard output
        printf("Primes below %d:\n", n);
        for (i = 0; i < gPrimeCount; i++) {
            printf("%d ", pPrimes[i]);
        }
        printf("\n");

    } else {

        // (b) large n: text file
        FILE *pFile = fopen(OUTPUT_FILE, "w");
        if (pFile == NULL) {
            printf("Could not open %s for writing.\n", OUTPUT_FILE);
            retval = 1;
            goto cleanup;
        }
        for (i = 0; i < gPrimeCount; i++) {
            fprintf(pFile, "%d\n", pPrimes[i]);
        }
        fclose(pFile);
        printf("Wrote %d primes to %s\n", gPrimeCount, OUTPUT_FILE);
    }

    // ---- load balance report (printed after every timer has been read) ---------------------------------------------------
    double busiest = 0.0, idlest = 1e9, sum = 0.0;
    for (i = 0; i < numThreads; i++) {
        printf("Thread %2d: %8d primes in [%10d, %10d)  %.4f s\n",
               pArgs[i].threadNum, pArgs[i].primeCount,
               pArgs[i].startVal, pArgs[i].endVal, pArgs[i].timeTaken);
        sum += pArgs[i].timeTaken;
        if (pArgs[i].timeTaken > busiest) busiest = pArgs[i].timeTaken;
        if (pArgs[i].timeTaken < idlest)  idlest  = pArgs[i].timeTaken;
    }
    if (busiest > 0.0 && idlest > 0.0) {
        printf("Load balance: busiest %.4f s, idlest %.4f s, ratio %.2fx, efficiency %.0f%%\n",
               busiest, idlest, busiest / idlest, 100.0 * (sum / numThreads) / busiest);
    }

    // one-line summary for tabulating results
    printf("n=%d threads=%d primes=%d base=%.4f s search=%.4f s total=%.4f s\n",
           n, numThreads, gPrimeCount, tBase, tSearch, tTotal);

cleanup:
    if (mutexReady) {
        pthread_mutex_destroy(&gMutex);
    }
    free(pPrimes);
    pPrimes = NULL;
    free(pArgs);
    pArgs = NULL;
    free(pTid);
    pTid = NULL;
    free(gIsComposite);
    gIsComposite = NULL;
    free(gBasePrimes);
    gBasePrimes = NULL;

    return retval;
}
