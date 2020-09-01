#include <mpi.h>
#include <stdio.h>

struct valuestruct {
    int a;
    double b;
};

int main(int argc, char** argv) {
    struct valuestruct values;
    int myrank;
    char packbuf[100];
    int position = 0;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    do {
        if (myrank == 0) {
            printf("Enter an round number (>0) & a real number: ");
            fflush(stdout);
            scanf("%d%lf", &values.a, &values.b);
        }
        // pack values one by one
        position = 0;
        MPI_Pack(&values.a, 1, MPI_INT, packbuf, 100, &position,
                 MPI_COMM_WORLD);
        MPI_Pack(&values.b, 1, MPI_DOUBLE, packbuf, 100, &position,
                 MPI_COMM_WORLD);
        // broadcast the packed buffer
        MPI_Bcast(packbuf, position, MPI_PACKED, 0, MPI_COMM_WORLD);
        // unpack values one by one
        position = 0;
        MPI_Unpack(packbuf, 100, &position, &values.a, 1, MPI_INT,
                   MPI_COMM_WORLD);
        MPI_Unpack(packbuf, 100, &position, &values.b, 1, MPI_DOUBLE,
                   MPI_COMM_WORLD);
        printf("Rank: %d. values.a = %d. values.b = %lf\n", myrank, values.a,
               values.b);
        fflush(stdout);
        // wait for all processes to complete before next iteration
        MPI_Barrier(MPI_COMM_WORLD);
    } while (values.a > 0);
    MPI_Finalize();
    return 0;
}
