#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct PrimeCalcInfo {
    long start;
    long increment;
    long end;
    long* ptr;
    size_t count;
} PrimeCalcInfo;

int is_prime(long i);
int write_to_file(long* arr, size_t len);
void* find_primes_thread(void* arg);
size_t find_primes(long start, long end, long increment, long* ptr);

int main(int argc, char* argv[]) {
    struct timespec start, finish;
    double elapsed, total_time = 0;
    clock_gettime(CLOCK_MONOTONIC, &start);
    double NANOSECONDS_IN_SECOND = 1000000000.0;

    // n = search for primes < n
    // t = no. of threads
    if (argc != 3) {
        printf("Usage: %s n [t]\n", argv[0]);
        return 1;
    }

    // get n
    char* ptr;
    long n = strtol(argv[1], &ptr, 10);
    if (ptr == argv[1]) {
        printf("Couldn't convert n\n");
        return 1;
    }

    // get number of threads
    ptr = NULL;
    long number_threads = strtol(argv[2], &ptr, 10);
    if (ptr == argv[2]) {
        printf("Couldn't convert t\n");
        return 1;
    }

    if (n <= 2) {
        printf("No primes less than 2\n");
        return 0;
    }

    // array to store primes
    // upper limit to no. of primes < n is n/2
    // there's probably a tighter upper bound but this is good enough
    long* primes = (long*)malloc((size_t)(n / 2) * sizeof(*primes));
    if (!primes) {
        printf("Could not malloc\n");
        return 1;
    }

    primes[0] = 2;
    size_t primes_count = 1;

    clock_gettime(CLOCK_MONOTONIC, &finish);
    elapsed = (finish.tv_sec - start.tv_sec) +
              (finish.tv_nsec - start.tv_nsec) / NANOSECONDS_IN_SECOND;
    total_time += elapsed;
    printf("Initial setup: %.3f s\n", elapsed);
    clock_gettime(CLOCK_MONOTONIC, &start);

    // info that each thread needs for their computations
    pthread_t tid[number_threads];
    PrimeCalcInfo* infos =
        (PrimeCalcInfo*)malloc((size_t)number_threads * sizeof(*infos));
    printf("Spawning %ld threads\n", number_threads);
    for (int i = 0; i < number_threads; ++i) {
        // multiply by 2 to ignore even numbers
        // each thread will start with a different number
        infos[i].start = 3 + 2 * i;
        infos[i].end = n;
        // each thread jumps forward the same number, hence will cover all
        // numbers
        infos[i].increment = 2 * number_threads;
        // each thread will have their own array to store the found primes
        infos[i].ptr =
            (long*)malloc((size_t)(n / 2 / number_threads) * sizeof(long));
        pthread_create(&tid[i], NULL, find_primes_thread, &infos[i]);
    }
    printf("Joining\n");
    for (int i = 0; i < number_threads; ++i) {
        pthread_join(tid[i], NULL);
    }
    printf("Copying\n");
    // from each thread's array of primes, copy into the main array
    // simple copy, WILL NOT BE SORTED
    for (int i = 0; i < number_threads; ++i) {
        for (size_t k = 0; k < infos[i].count; ++k) {
            primes[primes_count++] = infos[i].ptr[k];
        }
        free(infos[i].ptr);
    }
    free(infos);

    clock_gettime(CLOCK_MONOTONIC, &finish);
    elapsed = (finish.tv_sec - start.tv_sec) +
              (finish.tv_nsec - start.tv_nsec) / NANOSECONDS_IN_SECOND;
    total_time += elapsed;
    printf("Finding primes: %.3f s\n", elapsed);
    clock_gettime(CLOCK_MONOTONIC, &start);

    if (write_to_file(primes, primes_count)) {
        return 1;
    }

    printf("Written\n");

    free(primes);

    clock_gettime(CLOCK_MONOTONIC, &finish);
    elapsed = (finish.tv_sec - start.tv_sec) +
              (finish.tv_nsec - start.tv_nsec) / NANOSECONDS_IN_SECOND;
    total_time += elapsed;
    printf("Writing to file: %.3f s\n", elapsed);
    clock_gettime(CLOCK_MONOTONIC, &start);

    printf("Found %ld primes less than %ld\n", primes_count, n);
    printf("Time taken: %.3f s\n", total_time);

    return 0;
}

size_t find_primes(long start, long end, long increment, long* ptr) {
    size_t count = 0;
    for (long i = start; i < end; i += increment) {
        if (is_prime(i)) {
            ptr[count++] = i;
        }
    }
    return count;
}

void* find_primes_thread(void* arg) {
    PrimeCalcInfo* info = (PrimeCalcInfo*)arg;
    info->count =
        find_primes(info->start, info->end, info->increment, info->ptr);
}

int is_prime(long i) {
    long l = (long)ceil(sqrt((double)i));
    for (long j = 2; j <= l; ++j) {
        if (i % j == 0) {
            return 0;  // found a divisor, not prime
        }
    }
    return 1;  // no divisors, is prime
}

int write_to_file(long* arr, size_t len) {
    FILE* f = fopen("primes.txt", "w");
    if (!f) {
        printf("Could not open file to write\n");
        return 1;
    }

    for (size_t i = 0; i < len; ++i) {
        fprintf(f, "%ld\n", arr[i]);
    }

    fclose(f);
    return 0;
}
