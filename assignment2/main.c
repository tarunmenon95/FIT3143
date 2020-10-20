#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "base.h"
#include "common.h"
#include "ground.h"

int main(int argc, char* argv[]) {
    int rows, cols, max_iterations, world_rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    // unique seed per process
    srand(time(NULL) + (world_rank * 10));

    // user must specify grid dimensions and max iterations in commandline args
    if (argc != 4) {
        // to avoid spamming with every process output
        if (world_rank == 0)
            printf("Usage: %s num_rows num_cols max_iterations\n", argv[0]);
        MPI_Finalize();
        exit(0);
    }

    // parse these dimensions, ensure are proper
    char* ptr;
    rows = (int)strtol(argv[1], &ptr, 10);
    if (ptr == argv[1]) {
        if (world_rank == 0)
            printf("Couldn't parse to a number: %s\n", argv[1]);
        MPI_Finalize();
        exit(0);
    }
    ptr = NULL;
    cols = (int)strtol(argv[2], &ptr, 10);
    if (ptr == argv[2]) {
        if (world_rank == 0)
            printf("Couldn't parse to a number: %s\n", argv[2]);
        MPI_Finalize();
        exit(0);
    }
    ptr = NULL;
    max_iterations = (int)strtol(argv[3], &ptr, 10);
    if (ptr == argv[3]) {
        if (world_rank == 0)
            printf("Couldn't parse to a number: %s\n", argv[3]);
        MPI_Finalize();
        exit(0);
    }
    if (rows < 1 || cols < 1) {
        if (world_rank == 0) printf("Rows and cols must be larger than 0\n");
        MPI_Finalize();
        exit(0);
    }
    // ensure enough processes in total (grid + 1 base station)
    if (rows * cols + 1 != size) {
        if (world_rank == 0)
            printf(
                "Must run with (rows * cols + 1) processes instead of %d "
                "processes\n",
                size);
        MPI_Finalize();
        exit(0);
    }

    MPI_Datatype ground_message_type;
    create_ground_message_type(&ground_message_type);

    MPI_Comm split_comm;
    MPI_Comm_split(MPI_COMM_WORLD, world_rank == size - 1, 0, &split_comm);
    if (world_rank == size - 1) {
        base_station(size - 1, max_iterations, rows, cols, ground_message_type);
    } else {
        ground_station(split_comm, size - 1, rows, cols, ground_message_type);
    }
    MPI_Type_free(&ground_message_type);
    MPI_Comm_free(&split_comm);
    MPI_Finalize();
    exit(0);
}
