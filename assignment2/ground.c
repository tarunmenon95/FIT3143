#include "ground.h"

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "common.h"

void ground_station(MPI_Comm split_comm, int base_station_world_rank, int rows,
                    int cols, MPI_Datatype ground_message_type) {
    int grid_dimensions = 2;
    int dimension_sizes[2] = {rows, cols};
    MPI_Comm grid_comm;
    int periods[2] = {0, 0};  // don't wrap around on any dimension
    int reorder = 1;
    MPI_Cart_create(split_comm, grid_dimensions, dimension_sizes, periods,
                    reorder, &grid_comm);

    double start_time;
    int iteration = 0;
    char buf = '\0';
    int grid_rank;
    int coords[2];
    int reading;
    // get coordinates of ground sensor in grid
    MPI_Comm_rank(grid_comm, &grid_rank);
    MPI_Cart_coords(grid_comm, grid_rank, grid_dimensions, coords);
    // [Top Bottom Left Right]
    // by dimensions order, negative then positive
    int neighbour_readings[4];
    int neighbour_ranks[4];
    int neighbour_coords[4][2];
    // get ranks of neighbours (according to grid_comm)
    MPI_Cart_shift(grid_comm, 0, 1, &neighbour_ranks[0],
                   &neighbour_ranks[1]);  // top, bottom
    MPI_Cart_shift(grid_comm, 1, 1, &neighbour_ranks[2],
                   &neighbour_ranks[3]);  // left, right
    // get the coords of neighbours
    for (int i = 0; i < 4; ++i) {
        // in case edge/corner case and don't have 4 neighbours
        if (neighbour_ranks[i] != MPI_PROC_NULL)
            MPI_Cart_coords(grid_comm, neighbour_ranks[i], grid_dimensions,
                            neighbour_coords[i]);
    }
    // to listen for base station bcast indicating termination
    // (only time base station will bcast hence data sent doesn't matter)
    MPI_Request bcast_req;
    MPI_Ibcast(&buf, 1, MPI_CHAR, base_station_world_rank, MPI_COMM_WORLD,
               &bcast_req);
    int bcast_received = 0;
    while (!bcast_received) {
        start_time = MPI_Wtime();

        // clear neighbour readings each iteration
        for (int i = 0; i < 4; ++i) neighbour_readings[i] = -1;
        reading = rand() % (1 + MAX_READING_VALUE);
        // gather readings from neighbours
        MPI_Neighbor_allgather(&reading, 1, MPI_INT, neighbour_readings, 1,
                               MPI_INT, grid_comm);

        if (reading >= READING_THRESHOLD) {
            // event detected, fill in ground message
            GroundMessage msg;
            msg.iteration = iteration;
            msg.reading = reading;
            msg.rank = grid_rank;
            msg.coords[0] = coords[0];
            msg.coords[1] = coords[1];

            int matching_neighbours = 0;
            // check neighbours
            for (int i = 0; i < 4; ++i) {
                if (neighbour_readings[i] != -1 &&
                    abs(reading - neighbour_readings[i]) <=
                        READING_DIFFERENCE) {
                    // found a matching neighbour (within tolerance)
                    // fill in their data
                    msg.neighbour_ranks[matching_neighbours] =
                        neighbour_ranks[i];
                    msg.neighbour_coords[matching_neighbours][0] =
                        neighbour_coords[i][0];
                    msg.neighbour_coords[matching_neighbours][1] =
                        neighbour_coords[i][1];
                    msg.neighbour_readings[matching_neighbours] =
                        neighbour_readings[i];
                    ++matching_neighbours;
                }
            }

            msg.matching_neighbours = matching_neighbours;
            // use to format datetime
            msg.time_since_epoch = (long)time(NULL);

            if (matching_neighbours >= 2) {
                msg.mpi_time = MPI_Wtime();
                // event with at least 2 matching neighbours, send to base
                // (should ideally) buffer hence won't block
                MPI_Send(&msg, 1, ground_message_type, base_station_world_rank,
                         EVENT_MSG_TAG, MPI_COMM_WORLD);
            }
        }

        sleep_until_interval(start_time, INTERVAL_MILLISECONDS);
        // fix sync issue...
        // in case one proc gets ahead and subsequently blocks at gather
        MPI_Barrier(grid_comm);
        MPI_Test(&bcast_req, &bcast_received, MPI_STATUS_IGNORE);
        ++iteration;
    }
    MPI_Wait(&bcast_req, MPI_STATUS_IGNORE);

    printf("Rank %d (%d, %d) exiting\n", grid_rank, coords[0], coords[1]);
    MPI_Comm_free(&grid_comm);
}