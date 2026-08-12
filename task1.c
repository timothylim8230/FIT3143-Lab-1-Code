#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#


 int *isPrimes_before_n(int n, int *count) {

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

            int *Primes = malloc(isPrimeCount * sizeof(int));
            if (Primes == NULL) {
                free(isPrime);
                return NULL;
            }

            int index = 0;
            for (int i = 2; i < n; i++) {
                if (isPrime[i]) {
                    Primes[index++] = i;
                }
            }

            free(isPrime);

            *count = isPrimeCount;
            return Primes;
            
            
        }

        for (int j = i + 1; j < n; j++) {
            if (j % i == 0) {
                isPrime[j] = false;
            }
        }



    }

}


int main() {
    int n;

    printf("Enter value of n to find isPrimes: ");

    if (scanf("%d", &n) != 1) {
        printf("Invalid input.\n");
        return 1;
    }

    int count = 0;
    int *isPrimes = isPrimes_before_n(n, &count);

    
    if (n < 100) {

        // small n: print to standard output
        
        for (int i = 0; i < count; i++) {
            printf("%d ", isPrimes[i]);
        }
       

    } else {

        // large n: write to a text file
        FILE *out = fopen("primes.txt", "w");
        if (out == NULL) {
            printf("Could not open primes.txt for writing.\n");
            free(isPrimes);
            return 1;
        }

        
        for (int i = 0; i < count; i++) {
            fprintf(out, "%d\n", isPrimes[i]);
        }

        fclose(out);
        
    }

    free(isPrimes);

    return 0;
}