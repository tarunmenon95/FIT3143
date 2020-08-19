#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

int main(int argc, char* argv[]) {
   
    //Get user input for n
    int n;
    scanf ("%d",&n);

    struct timespec startProgram, endProgram, startComp, endComp;
    
    //Get program start time
    clock_gettime(CLOCK_MONOTONIC, &startProgram); 
    
    int write_to_file(long* arr, size_t len);
    double totalTime;
     
     
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

    //Get computation start time
    clock_gettime(CLOCK_MONOTONIC, &startComp);
    int primes[n];

    //Find all primes and add to array
    int counter = 0;
    for (int i = 1; i <= n; i++){
        if (is_prime(i))
        {
            primes[counter] = i;
            counter++;
        }
    }
    
    //Get computation end time
    clock_gettime(CLOCK_MONOTONIC, &endComp);

    //Output computation time
    totalTime = (endComp.tv_sec - startComp.tv_sec) * 1e9; 
    totalTime= (totalTime + (endComp.tv_nsec - startComp.tv_nsec)) * 1e-9; 
    printf("%lf", totalTime);
    printf("%s", "\n");


    //Open output primes.txt file
    FILE* f = fopen("primes.txt", "w");

    for (int i = 0; i < counter; i++)
    {
        fprintf(f,"%d", primes[i]);
        fprintf(f, "\n");
    }

    fclose(f);


    //Get program end time and output
    clock_gettime(CLOCK_MONOTONIC, &endProgram);
    totalTime = (endProgram.tv_sec - startProgram.tv_sec) * 1e9; 
    totalTime= (totalTime + (endProgram.tv_nsec - startProgram.tv_nsec)) * 1e-9; 


    printf("%lf", totalTime);


   

}