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

    //2. ALLOCATE     bool *isPrime =  malloc( n * sizeof(bool));
    //                  check for NULL
    bool *isPrime = malloc( n * sizeof(bool));
    if (isPrime == NULL){
        return 1;
    }



    //4. START CLOCK   double t0 = now_seconds();

    double t0 = now_seconds();

    int sqrtN = sqrt((double)n) + 1;

    if (sqrtN > n - 1) {
        sqrtN = n - 1;
    }

    //5. THE PARALLEL REGION - this is the whole of task 3-----
    // #pragma omp parallel for    
    //    schedule( ???)
    //reduction(+:count)      
    //shared(isPrime)  private(...)
    //for (int c =2 ;  c < n;)
    //   isPrime[c]     = is_prime(c);
    // if   (isPrime[c]) count++
    //   }
    //      <- implicit barrier here. All threads done. No join needed


    //6. STOP SEARCH CLOCK           double searchtime = now_seconds()  - t0;
    
    
    //----- 7. COLLECT (SERIAL, and it must stay serial)  ------------
    // int *primes = malloc(count * sizeof(int));
    // walk c = 2 .... n -1   in order, append where isPrime[c]
    //-> ascending order falls out for free,  exactly as in task2


    //8. STOP TOTAL CLOCK double totalTime = now_seconds() - t0;


    //  ----- 9. OUTPUT

    return 0;
}


