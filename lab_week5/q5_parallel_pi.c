#include <math.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    int num_tasks, rank, n;
    const int root = 0;
    double pi_val = 0.0;
    double local_sum, start, end;

    MPI_Status stat;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &num_tasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // getting N from root
    if (rank == root) {
        printf("Enter n value:\n");
        fflush(stdout);
        scanf("%d", &n);
    }
    // start timing
    start = MPI_Wtime();
    // broadcast n to all other processes
    MPI_Bcast(&n, 1, MPI_INT, root, MPI_COMM_WORLD);

    // each process begins at their rank and increments by same amount
    // hence will cover a different iteration each
    for (int i = rank; i < n; i += num_tasks) {
        local_sum += 4.0 / (1 + pow((2.0 * i + 1.0) / (2.0 * n), 2));
    }
    // summing onto root
    MPI_Reduce(&local_sum, &pi_val, 1, MPI_DOUBLE, MPI_SUM, root,
               MPI_COMM_WORLD);

    end = MPI_Wtime();

    // only root will print and display time
    if (rank == root) {
        pi_val /= (double)n;

        printf("Calculated Pi value (Parallel-AlgoI) = %12.9f\n", pi_val);
        printf("Overall time (s): %lf\n", end - start);
        fflush(stdout);
    }
    MPI_Finalize();

    return 0;
}
