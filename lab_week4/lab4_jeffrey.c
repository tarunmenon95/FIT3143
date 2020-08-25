#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NUMBERS_LENGTH 10

// compile with -fopenmp flag
int main(int argc, char* argv[]) {
    int numbers[NUMBERS_LENGTH] = {0};

    // fill numbers
#pragma omp parallel num_threads(4)
    {
        // unique seed per thread
        int seed = (int)time(NULL) ^ omp_get_thread_num();
#pragma omp for schedule(static, 2)
        for (int i = 0; i < NUMBERS_LENGTH; ++i) {
            // omitting critical since each thread updates different indices
            numbers[i] = 1 + (rand_r(&seed) % 25);
        }
    }

    // display
    for (int i = 0; i < NUMBERS_LENGTH; ++i) {
        printf("%d ", numbers[i]);
    }
    printf("\n");

    // check wins
    int check[25] = {0};
    int wins = 0;
    for (int i = 0; i < NUMBERS_LENGTH; ++i) {
        check[numbers[i] - 1]++;
        if (check[numbers[i] - 1] == 2) {
            wins++;
            printf("Win: %d\n", numbers[i]);
        }
    }
    printf("Total wins: %d\n", wins);

    return 0;
}
