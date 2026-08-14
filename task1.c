#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

/**
 * Timothy Lim           tlim0034@student.monash.edu
 * Scott Nguyen 33879095 sngu0065@student.monash.edu
 */

 int *primes_before_n(int n, int *count) {


    *count = 0;

    // no primes below 2, and the loop below would never run
    if (n <= 2) {
        return NULL;
    }

    // get sqrt of n
    int sqrtN = sqrt(n);

    // initialise array to store if a number is isPrime or not
    // (heap, not a stack array: n can be in the millions)
    bool *isPrime = malloc((n + 1) * sizeof(bool));
    if (isPrime == NULL) {
        return NULL;
    }

    // set isPrime array to all true 
     for (int i = 0; i <= n; i++) {
        isPrime[i] = true;
     }

     // set 0 and 1 to false 
     isPrime[0] = isPrime[1] = false;

    // loop through all values below i until sqrt(n) to cross off non-isPrimes
    for (int i = 2; i < n; i++) {
        
        if (i > sqrtN) {

            // initialise isPrime counts 
            int isPrimeCount = 0;
            
            // increment isPrimeCount for each true in isPrime array
            for (int i = 2; i < n; i++) {
                if (isPrime[i]) {
                    isPrimeCount++;
                }
            }

            int *primes = malloc(isPrimeCount * sizeof(int));
            if (primes == NULL) {
                free(isPrime);
                return NULL;
            }

            int index = 0;
            for (int i = 2; i < n; i++) {
                if (isPrime[i]) {
                    primes[index++] = i;
                }
            }

            free(isPrime);

            *count = isPrimeCount;
            return primes;
            
            
        }

    // composite i: its multiples were already crossed off by i's prime factors
    if (!isPrime[i]) continue;

    // start at i*i: any smaller multiple of i already has a smaller
    // prime factor and was crossed off in an earlier pass
    for (int j = i * i; j < n; j += i) {
        isPrime[j] = false;
    }


    }

}


int main() {
    int n;
    clock_t t;

    printf("Enter value of n to find isPrimes: ");

    if (scanf("%d", &n) != 1) {
        printf("Invalid input.\n");
        return 1;
    }

    
    

    int count = 0;

    t = clock(); // record time before execution

    int *isPrimes = primes_before_n(n, &count);

    t = clock() - t; // get time elapsed since last time recorded after execution

    double timeTaken = ((double)t)/CLOCKS_PER_SEC; // get the time in seconds

    printf("Time taken to compute: %f seconds\n", timeTaken);
    
    if (n < 100) {

        // small n: print to standard output
        printf("Primes below %d:\n", n);
        for (int i = 0; i < count; i++) {
            printf("%d ", isPrimes[i]);
        }
        printf("\n");

    } else {

        // large n: write to a text file
        FILE *out = fopen("primes1.txt", "w");
        if (out == NULL) {
            printf("Could not open primes1.txt for writing.\n");
            free(isPrimes);
            return 1;
        }

        
        for (int i = 0; i < count; i++) {
            fprintf(out, "%d\n", isPrimes[i]);
        }

        fclose(out);
        printf("Primes below n written to primes1.txt\n");
    }

    
    free(isPrimes);

    return 0;
}