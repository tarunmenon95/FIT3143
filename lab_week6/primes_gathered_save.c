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

    // each process gets own array to store primes found
    long* arr = (long*)malloc((size_t)(n / 2 / num_tasks) * sizeof(*arr));
    // number of elements in array
    size_t len = 0;
    // give 2 to root as known prime
    if (rank == root) {
        arr[len++] = 2;
    }
    // look for primes
    for (long i = 3 + 2 * rank; i < n; i += 2 * num_tasks) {
        if (is_prime(i)) {
            arr[len++] = i;
        }
    }

    // on root, allocate these arrays
    // all_len will be array representing number of primes found by each process
    // displacements will be array representing where to start copying the
    // received primes from each process into main array on root
    int *all_len, *displacements;
    if (rank == root) {
        all_len = (int*)malloc(num_tasks * sizeof(*all_len));
        displacements = (int*)malloc(num_tasks * sizeof(*displacements));
    }
    // initial gather to know how many primes each process found
    MPI_Gather(&len, 1, MPI_INT, all_len, 1, MPI_INT, root, MPI_COMM_WORLD);

    long* all_arr;
    int total_len = 0;
    if (rank == root) {
        // on root, sum these lengths to allocate array big enough to hold all
        // primes
        for (int i = 0; i < num_tasks; ++i) {
            total_len += all_len[i];
        }
        all_arr = (long*)malloc(total_len * sizeof(*all_arr));
        // calculate the displacements from which each processes' primes will
        // start being copied into main array
        displacements[0] = 0;
        for (int i = 1; i < num_tasks; ++i) {
            displacements[i] = displacements[i - 1] + all_len[i - 1];
        }
    }

    // gather variable number of data from processes onto root
    MPI_Gatherv(arr, len, MPI_LONG, all_arr, all_len, displacements, MPI_LONG,
                root, MPI_COMM_WORLD);

    // write out from root and deallocate root specific resources
    if (rank == root) {
        FILE* f = fopen("primes.txt", "w");
        if (!f) {
            printf("Could not open file to write\n");
            return 1;
        }
        for (size_t i = 0; i < total_len; ++i) {
            fprintf(f, "%ld\n", all_arr[i]);
        }
        fclose(f);
        free(all_arr);
        free(all_len);
        free(displacements);
    }
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
