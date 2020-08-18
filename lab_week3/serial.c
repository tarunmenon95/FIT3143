#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char* argv[]) {
    
    struct timespec start, end;
   
    double totalTime;

    clock_gettime(CLOCK_MONOTONIC, &start);

    //Get user input for n
    int n;
    scanf ("%d",&n);
    

    
    
    totalTime = end.tv_sec - start.tv_sec;
    clock_gettime(CLOCK_MONOTONIC, &start);


    printf("%lf",totalTime);
    

}