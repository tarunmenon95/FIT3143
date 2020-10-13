#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "mpi.h"

#define GRID_DIMENSIONS 2
#define INTERVAL_MILLISECONDS 1000
#define READING_THRESHOLD 80
#define MAX_READING_VALUE 100
#define SECONDS_TO_NANOSECONDS 1000000000

typedef struct {
    int coords[2];
    int reading;
    double timestamp;
} SatelliteReading;

// pointer to next spot to replace in infrared_readings array
int infrared_readings_latest = 0;
SatelliteReading infrared_readings[100];

int main(int argc, char* argv[]) {
    int rows, cols, max_iterations;
    int world_rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    // unique seed per process
    srand(time(NULL) + world_rank);

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

    int dimension_sizes[2] = {rows, cols};
    MPI_Comm grid_comm;
    int periods[2] = {0, 0};  // don't wrap around on any dimension
    int reorder = 1;
    MPI_Cart_create(MPI_COMM_WORLD, GRID_DIMENSIONS, dimension_sizes, periods,
                    reorder, &grid_comm);

    int base_station_world_rank = size - 1;
    double start_time, end_time, sleep_length;
    struct timespec ts;
    int iteration = 0;
    int is_base = grid_comm == MPI_COMM_NULL;
    char terminate_msg = is_base ? 't' : '\0';
    int grid_rank;
    if (!is_base) MPI_Comm_rank(grid_comm, &grid_rank);
    int coords[2];
    int reading;
    // get coordinates of ground sensor in grid
    if (!is_base)
        MPI_Cart_coords(grid_comm, grid_rank, GRID_DIMENSIONS, coords);
    // [Top Bottom Left Right]
    // by dimensions order, negative then positive
    int neighbour_readings[4];
    int bcast_casted = 0;
    int bcast_received = 0;
    MPI_Request bcast_req;
    while (is_base && iteration < max_iterations ||
           !is_base && !bcast_received) {
        start_time = MPI_Wtime();

        ++iteration;

        for (int i = 0; i < 4; ++i) neighbour_readings[i] = -1;
        reading = rand() % (1 + MAX_READING_VALUE);

        if (!is_base)
            MPI_Neighbor_allgather(&reading, 1, MPI_INT, neighbour_readings, 1,
                                   MPI_INT, grid_comm);

        if (is_base) {
            printf("[%d] Base|%.9f\n", iteration, start_time);
        } else {
            printf("[%d] %d:(%d,%d)|%d|%d,%d,%d,%d|%.9f\n", iteration,
                   grid_rank, coords[0], coords[1], reading,
                   neighbour_readings[0], neighbour_readings[1],
                   neighbour_readings[2], neighbour_readings[3], start_time);
        }

        if (is_base && iteration >= max_iterations ||
            !is_base && !bcast_casted) {
            MPI_Ibcast(&terminate_msg, 1, MPI_CHAR, base_station_world_rank,
                       MPI_COMM_WORLD, &bcast_req);
            if (!is_base) bcast_casted = 1;
            if (is_base) MPI_Wait(&bcast_req, MPI_STATUS_IGNORE);
        }
        MPI_Barrier(MPI_COMM_WORLD);
        if (!is_base) MPI_Test(&bcast_req, &bcast_received, MPI_STATUS_IGNORE);

        end_time = MPI_Wtime();
        sleep_length =
            (start_time + ((double)INTERVAL_MILLISECONDS / 1000) - end_time) *
            SECONDS_TO_NANOSECONDS;
        ts.tv_sec = 0;
        ts.tv_nsec = (long)sleep_length;
        nanosleep(&ts, NULL);
    }

    if (is_base) {
        printf("Base station exiting\n");
    } else {
        printf("Rank %d (%d, %d) exiting\n", grid_rank, coords[0], coords[1]);
    }

    MPI_Comm_free(&grid_comm);
    MPI_Finalize();
    exit(0);
}
