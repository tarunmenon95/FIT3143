#include <stdio.h>
#include <stdlib.h>

#include "mpi.h"

#define GRID_DIMENSIONS 2
#define INTERVAL_MILLISECONDS 100
#define READING_THRESHOLD 80

typedef struct {
    int coords[2];
    int reading;
    double timestamp;
} SatelliteReading;

// pointer to next spot to replace in infrared_readings array
int infrared_readings_latest = 0;
SatelliteReading infrared_readings[100];

int main(int argc, char* argv[]) {
    int rows, cols;
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // user must specify grid dimensions in commandline args
    if (argc != 3) {
        // to avoid spamming with every process output
        if (rank == 0) printf("Usage: %s num_rows num_cols\n", argv[0]);
        MPI_Finalize();
        exit(0);
    }

    // parse these dimensions, ensure are proper
    char* ptr;
    rows = (int)strtol(argv[1], &ptr, 10);
    if (ptr == argv[1]) {
        if (rank == 0) printf("Couldn't parse to a number: %s\n", argv[1]);
        MPI_Finalize();
        exit(0);
    }
    ptr = NULL;
    cols = (int)strtol(argv[2], &ptr, 10);
    if (ptr == argv[2]) {
        if (rank == 0) printf("Couldn't parse to a number: %s\n", argv[2]);
        MPI_Finalize();
        exit(0);
    }
    if (rows < 1 || cols < 1) {
        if (rank == 0) printf("Rows and cols must be larger than 0\n");
        MPI_Finalize();
        exit(0);
    }
    // ensure enough processes in total (grid + 1 base station)
    if (rows * cols + 1 != size) {
        if (rank == 0)
            printf(
                "Must run with (rows * cols + 1) processes instead of %d "
                "processes\n",
                size);
        MPI_Finalize();
        exit(0);
    }

    int dimension_sizes[2] = {rows, cols};
    MPI_Comm grid_comm;
    int periods[2] = {0, 0};  // don't wrap around on any dimension
    int reorder = 1;
    MPI_Cart_create(MPI_COMM_WORLD, GRID_DIMENSIONS, dimension_sizes, periods,
                    reorder, &grid_comm);

    if (grid_comm == MPI_COMM_NULL) {
        // base station
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        printf("I am root: %d\n", rank);
    } else {
        // ground sensor
        MPI_Comm_rank(grid_comm, &rank);
        int coords[2];
        // get coordinates of this ground sensor
        MPI_Cart_coords(grid_comm, rank, GRID_DIMENSIONS, coords);
        printf("I am ground sensor (%d, %d): %d\n", coords[0], coords[1], rank);
    }

    MPI_Finalize();
    exit(0);
}
