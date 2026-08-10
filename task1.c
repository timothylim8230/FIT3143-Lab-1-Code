#include <stdio.h>;
#include <math.h>;



void primes_before_n(int n) {

    // get sqrt of n 
    int sqrtN = sqrt(n);

    // initialise array to store if a number is prime or not
    bool isPrime[n+1];

    // set prime array to all true 
     for (int i = 0; i <= n; i++) {
        isPrime[i] = true;
     }

    // loop through all values below i until sqrt(n) to cross off non-primes
    for (int i = 0; i < n; i++) {
        
        if (i >= sqrtN) {

            // initialise prime counts 
            int primeCount = 0;
            
            // increment primeCount for each true in prime array
            for (int i = 0; i < n; i++) {
                if (isPrime[i]) {
                    primeCount++;
                }
            }

            int primes[primeCount];
            int index = 0;
            for (int i = 1; i <= n; i++) {
                if (isPrime[i]) {
                    primes[index++] = i;
                }
            }

            return primes;
            
            
        }


    }

}


int main() {





}