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
    // t = no. of threads
    if (argc != 3) {
        printf("Usage: %s n [t]\n", argv[0]);
        return 1;
    }

    // get N
    char* ptr;
    long n = strtol(argv[1], &ptr, 10);
    if (ptr == argv[1]) {
        printf("Couldn't convert n\n");
        return 1;
    }

    // if parallel get number of threads
    long number_threads;
    ptr = NULL;
    number_threads = strtol(argv[2], &ptr, 10);
    if (ptr == argv[2]) {
        printf("Couldn't convert t\n");
        return 1;
    }

    if (n <= 2) {
        printf("No primes less than 2\n");
        return 0;
    }

    long* primes = (long*)malloc((size_t)(n / 2) * sizeof(*primes));
    if (!primes) {
        printf("Could not malloc\n");
        return 1;
    }

    primes[0] = 2;
    size_t primes_count = 1;

    struct timespec start, finish;
    double elapsed;
    clock_gettime(CLOCK_MONOTONIC, &start);
    pthread_t tid[number_threads];
    PrimeCalcInfo* infos =
        (PrimeCalcInfo*)malloc((size_t)number_threads * sizeof(*infos));
    printf("Spawning %ld threads\n", number_threads);
    for (int i = 0; i < number_threads; ++i) {
        infos[i].start = 3 + 2 * i;
        infos[i].end = n;
        infos[i].increment = 2 * number_threads;
        infos[i].ptr =
            (long*)malloc((size_t)(n / 2 / number_threads) * sizeof(long));
        pthread_create(&tid[i], NULL, find_primes_thread, &infos[i]);
    }
    printf("Joining\n");
    for (int i = 0; i < number_threads; ++i) {
        pthread_join(tid[i], NULL);
    }
    printf("Copying\n");
    for (int i = 0; i < number_threads; ++i) {
        for (size_t k = 0; k < infos[i].count; ++k) {
            primes[primes_count + k] = infos[i].ptr[k];
        }
        free(infos[i].ptr);
        primes_count += infos[i].count;
    }
    free(infos);

    clock_gettime(CLOCK_MONOTONIC, &finish);
    elapsed = (finish.tv_sec - start.tv_sec);
    elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;

    printf("Found %ld primes less than %ld\nTime taken: %.2f s\n", primes_count,
           n, elapsed);

    if (write_to_file(primes, primes_count)) {
        return 1;
    }

    printf("Written\n");

    free(primes);

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
            return 0;  // not prime
        }
    }
    return 1;  // is prime
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
