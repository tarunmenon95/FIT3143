#include <math.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

int is_prime(long i);

int main(int argc, char* argv[]) {
    int num_tasks, rank;
    long n;
    const int root = 0;
    double start, end;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &num_tasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // getting N from root
    if (rank == root) {
        printf("Enter n value:\n");
        fflush(stdout);
        scanf("%ld", &n);
    }
    // start timing
    start = MPI_Wtime();
    // broadcast n to all other processes
    MPI_Bcast(&n, 1, MPI_LONG, root, MPI_COMM_WORLD);

    // each process gets array to store primes found
    long* arr = (long*)malloc((size_t)(n / 2 / num_tasks) * sizeof(*arr));
    // counts how many elements currently in the array
    size_t len = 0;
    // special case for root: it gets known prime 2 as first element
    // since we start looking for primes starting at 3
    if (rank == root) {
        arr[len++] = 2;
    }
    // each process starts at different int according to their rank
    // and leap by 2 (since even numbers aren't prime except 2) multiplied by
    // num_tasks to ensure each process jumps to different number and all
    // numbers are covered
    for (long i = 3 + 2 * rank; i < n; i += 2 * num_tasks) {
        if (is_prime(i)) {
            arr[len++] = i;
        }
    }

    // build filename for each process
    char filename[32];
    snprintf(filename, sizeof filename, "primes_%d.txt", rank);
    FILE* f = fopen(filename, "w");
    if (!f) {
        printf("Rank %d: Could not open file to write: %s\n", rank, filename);
        return 1;
    }
    // each process will write to unique file
    for (size_t i = 0; i < len; ++i) {
        fprintf(f, "%ld\n", arr[i]);
    }
    fclose(f);
    free(arr);

    MPI_Barrier(MPI_COMM_WORLD);
    end = MPI_Wtime();

    if (rank == root) {
        printf("Overall time (s): %lf\n", end - start);
        fflush(stdout);
    }
    MPI_Finalize();

    return 0;
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
