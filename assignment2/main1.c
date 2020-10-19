#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define INTERVAL_MILLISECONDS 1000
#define READING_THRESHOLD 70   // 80
#define READING_DIFFERENCE 20  // 5
#define MAX_READING_VALUE 100
#define SECONDS_TO_NANOSECONDS 1000000000
#define EVENT_MSG_TAG 0

void base_station(int, int, int, int, MPI_Datatype);
void ground_station(MPI_Comm, int, int, int, MPI_Datatype);
void create_ground_message_type(MPI_Datatype*);
void sleep_until(double);
void* infrared_thread(void*);

typedef struct {
    int coords[2];
    int reading;
    double timestamp;
} SatelliteReading;

typedef struct {
    int iteration;
    int reading;
    int rank;
    int coords[2];
    int matching_neighbours;  // size of neighbour_* arrs
    int neighbour_ranks[4];
    int neighbour_coords[4][2];
    int neighbour_readings[4];
    long time_since_epoch;  // when this event occurred
} GroundMessage;

// pointer to next spot to replace in infrared_readings array
int infrared_readings_latest = 0;
SatelliteReading infrared_readings[50];

int terminate = 0;

int main(int argc, char* argv[]) {
    int rows, cols, max_iterations, world_rank, size;
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

void base_station(int base_station_world_rank, int max_iterations, int rows,
                  int cols, MPI_Datatype ground_message_type) {
    double start_time;
    int dimension_sizes[2] = {rows, cols};
    int iteration = 0;
    char buf = '\0';
    MPI_Request bcast_req;
    pthread_t tid;
    pthread_create(&tid, NULL, infrared_thread, (void*)dimension_sizes);
    int messages_available;
    while (iteration < max_iterations) {
        start_time = MPI_Wtime();

        MPI_Iprobe(MPI_ANY_SOURCE, EVENT_MSG_TAG, MPI_COMM_WORLD,
                   &messages_available, MPI_STATUS_IGNORE);
        while (messages_available) {
            GroundMessage msg;
            MPI_Recv(&msg, 1, ground_message_type, MPI_ANY_SOURCE,
                     EVENT_MSG_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            printf("Event from %d: %d\n", msg.rank, msg.reading);

            MPI_Iprobe(MPI_ANY_SOURCE, EVENT_MSG_TAG, MPI_COMM_WORLD,
                       &messages_available, MPI_STATUS_IGNORE);
        }

        printf("[%d] Base|%.9f\n", iteration, start_time);

        sleep_until(start_time);
        ++iteration;
    }

    terminate = 1;
    MPI_Ibcast(&buf, 1, MPI_CHAR, base_station_world_rank, MPI_COMM_WORLD,
               &bcast_req);
    MPI_Wait(&bcast_req, MPI_STATUS_IGNORE);
    pthread_join(tid, NULL);
    printf("Base station exiting\n");
}

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
    MPI_Cart_shift(grid_comm, 0, 1, &neighbour_ranks[0],
                   &neighbour_ranks[1]);  // top, bottom
    MPI_Cart_shift(grid_comm, 1, 1, &neighbour_ranks[2],
                   &neighbour_ranks[3]);  // left, right
    for (int i = 0; i < 4; ++i) {
        if (neighbour_ranks[i] != MPI_PROC_NULL)
            MPI_Cart_coords(grid_comm, neighbour_ranks[i], grid_dimensions,
                            neighbour_coords[i]);
    }
    MPI_Request bcast_req;
    MPI_Ibcast(&buf, 1, MPI_CHAR, base_station_world_rank, MPI_COMM_WORLD,
               &bcast_req);
    while (!terminate) {
        start_time = MPI_Wtime();

        for (int i = 0; i < 4; ++i) neighbour_readings[i] = -1;
        reading = rand() % (1 + MAX_READING_VALUE);
        MPI_Neighbor_allgather(&reading, 1, MPI_INT, neighbour_readings, 1,
                               MPI_INT, grid_comm);

        if (reading >= READING_THRESHOLD) {
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
            msg.time_since_epoch = (long)time(NULL);

            if (matching_neighbours >= 2) {
                // event, send to base
                printf("%d sending\n", grid_rank);
                MPI_Send(&msg, 1, ground_message_type, base_station_world_rank,
                         EVENT_MSG_TAG, MPI_COMM_WORLD);
                printf("[%d] %d:(%d,%d)|%d|%d,%d,%d,%d|%.9f\n", iteration,
                       grid_rank, coords[0], coords[1], reading,
                       neighbour_readings[0], neighbour_readings[1],
                       neighbour_readings[2], neighbour_readings[3],
                       start_time);
            }
        }

        sleep_until(start_time);
        // fix sync issue...
        // in case one proc gets ahead and subsequently blocks at gather
        MPI_Barrier(grid_comm);
        MPI_Test(&bcast_req, &terminate, MPI_STATUS_IGNORE);
        ++iteration;
    }
    MPI_Wait(&bcast_req, MPI_STATUS_IGNORE);

    printf("Rank %d (%d, %d) exiting\n", grid_rank, coords[0], coords[1]);
    MPI_Comm_free(&grid_comm);
}

void create_ground_message_type(MPI_Datatype* ground_message_type) {
    // create MPI datatype for GroundMessage
    int no_of_fields = 9;
    int block_lengths[9] = {1, 1, 1, 2, 1, 4, 4 * 2, 4, 1};
    MPI_Aint displacements[9];
    displacements[0] = offsetof(GroundMessage, iteration);
    displacements[1] = offsetof(GroundMessage, reading);
    displacements[2] = offsetof(GroundMessage, rank);
    displacements[3] = offsetof(GroundMessage, coords);
    displacements[4] = offsetof(GroundMessage, matching_neighbours);
    displacements[5] = offsetof(GroundMessage, neighbour_ranks);
    displacements[6] = offsetof(GroundMessage, neighbour_coords);
    displacements[7] = offsetof(GroundMessage, neighbour_readings);
    displacements[8] = offsetof(GroundMessage, time_since_epoch);
    MPI_Datatype datatypes[9] = {MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT,
                                 MPI_INT, MPI_INT, MPI_INT, MPI_LONG};
    MPI_Type_create_struct(no_of_fields, block_lengths, displacements,
                           datatypes, ground_message_type);
    MPI_Type_commit(ground_message_type);
}

void sleep_until(double start_time) {
    // sleep until INTERVAL_MILLISECONDS has passed since start_time
    struct timespec ts;
    double end_time = MPI_Wtime();
    double sleep_length =
        (start_time + ((double)INTERVAL_MILLISECONDS / 1000) - end_time) *
        SECONDS_TO_NANOSECONDS;
    ts.tv_sec = 0;
    ts.tv_nsec = (long)sleep_length;
    nanosleep(&ts, NULL);
}

void* infrared_thread(void* arg) {
    double start_time;
    int* dimensions = (int*)arg;
    int rows = dimensions[0];
    int cols = dimensions[1];
    while (!terminate) {
        start_time = MPI_Wtime();
        printf("Thread heartbeat\n");
        sleep_until(start_time);
    }
    printf("Thread terminating\n");
}
