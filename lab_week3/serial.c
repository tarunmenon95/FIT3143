#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

int main(int argc, char* argv[]) {
    int write_to_file(long* arr, size_t len);
    struct timespec start, end;
    double totalTime;


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
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    int primes[n];

    int counter = 0;
    for (int i = 1; i <= n; i++){
        if (is_prime(i))
        {
            primes[counter] = i;
            counter++;
        }
    }

    FILE* f = fopen("primes.txt", "w");

    for (int i = 0; i < counter; i++)
    {
        fprintf(f,"%d", primes[i]);
        fprintf(f, "\n");
    }

    fclose(f);

    clock_gettime(CLOCK_MONOTONIC, &end);

   totalTime = (end.tv_sec - start.tv_sec) * 1e9; 
   totalTime= (totalTime + (end.tv_nsec - start.tv_nsec)) * 1e-9; 

   printf("%lf", totalTime);

}