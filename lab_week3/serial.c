#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

int main(int argc, char* argv[]) {
    int write_to_file(long* arr, size_t len);
    struct timespec start, end;
    double totalTime;


    clock_gettime(CLOCK_MONOTONIC, &start);

    //Get user input for n
    int n;
    scanf ("%d",&n);
    
    //Check whether num is prime
    int is_prime(long i) {
    long l = (long)ceil(sqrt((double)i));
    for (long j = 2; j <= l; ++j) {
        if (i % j == 0) {
            return 0;  // not prime
        }
    }
    return 1;  // is prime
    }

    //Count primes
    int primecount = 0;

    for (int i = 1; i <= n; i++){
        if (is_prime(i))
        {
            primecount++;
        }
    }

    int primes[primecount];

    int counter = 0;
    for (int i = 1; i <= n; i++){
        if (is_prime(i))
        {
            primes[counter] = i;
            counter++;
        }
    }


    FILE* f = fopen("primes.txt", "w");

    for (int i = 0; i < primecount; i++)
    {
        fprintf(f,"%d", primes[i]);
        fprintf(f, "\n");
    }

    fclose(f);
    




    totalTime = end.tv_sec - start.tv_sec;
    clock_gettime(CLOCK_MONOTONIC, &start);


}