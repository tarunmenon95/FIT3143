#include "common.h"

#include <mpi.h>
#include <time.h>

void create_ground_message_type(MPI_Datatype* ground_message_type) {
    // create MPI datatype for GroundMessage
    int no_of_fields = 10;
    int block_lengths[10] = {1, 1, 1, 2, 1, 4, 4 * 2, 4, 1, 1};
    MPI_Aint displacements[10];
    displacements[0] = offsetof(GroundMessage, iteration);
    displacements[1] = offsetof(GroundMessage, reading);
    displacements[2] = offsetof(GroundMessage, rank);
    displacements[3] = offsetof(GroundMessage, coords);
    displacements[4] = offsetof(GroundMessage, matching_neighbours);
    displacements[5] = offsetof(GroundMessage, neighbour_ranks);
    displacements[6] = offsetof(GroundMessage, neighbour_coords);
    displacements[7] = offsetof(GroundMessage, neighbour_readings);
    displacements[8] = offsetof(GroundMessage, mpi_time);
    displacements[9] = offsetof(GroundMessage, time_since_epoch);
    MPI_Datatype datatypes[10] = {MPI_INT,    MPI_INT, MPI_INT, MPI_INT,
                                  MPI_INT,    MPI_INT, MPI_INT, MPI_INT,
                                  MPI_DOUBLE, MPI_LONG};
    MPI_Type_create_struct(no_of_fields, block_lengths, displacements,
                           datatypes, ground_message_type);
    MPI_Type_commit(ground_message_type);
}

void sleep_until_interval(double start_time, int interval_ms) {
    // sleep until interval_ms has passed since start_time
    struct timespec ts;
    double end_time = MPI_Wtime();
    double sleep_length =
        (start_time + ((double)interval_ms / 1000) - end_time) *
        SECONDS_TO_NANOSECONDS;
    ts.tv_sec = 0;
    ts.tv_nsec = (long)sleep_length;
    nanosleep(&ts, NULL);
}
