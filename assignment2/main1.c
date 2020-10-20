#include <math.h>
#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

#define INTERVAL_MILLISECONDS 1000
// what constitutes an event reading
#define READING_THRESHOLD 70  // 80
// allowable absolute diff b/w readings
#define READING_DIFFERENCE 20  // 5
// allowable absolute diff b/w mpi times for event & thread reading
#define MPI_TIME_DIFF_MILLISECONDS 400
#define MAX_READING_VALUE 100
#define SECONDS_TO_NANOSECONDS 1000000000
#define EVENT_MSG_TAG 0

typedef struct {
    int coords[2];
    int reading;
    double mpi_time;
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
    double mpi_time;        // when the event occured
    long time_since_epoch;  // to format as datetime
} GroundMessage;

void base_station(int, int, int, int, MPI_Datatype);
void ground_station(MPI_Comm, int, int, int, MPI_Datatype);
void create_ground_message_type(MPI_Datatype*);
void sleep_until_interval(double, int);
void* infrared_thread(void*);
void generate_satellite_reading(SatelliteReading*, int, int);
int file_exists(const char*);
int compare_satellite_readings(GroundMessage*, SatelliteReading*);

// global arr for thread to store its satellite readings
SatelliteReading infrared_readings[30];

// flag to indicate whether thread should terminate
int terminate = 0;

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

void base_station(int base_station_world_rank, int max_iterations, int rows,
                  int cols, MPI_Datatype ground_message_type) {
    double start_time;
    int dimension_sizes[2] = {rows, cols};
    int iteration = 0;
    // doesn't matter what we send in bcast
    char buf = '\0';
    MPI_Request bcast_req;
    pthread_t tid;
    pthread_create(&tid, NULL, infrared_thread, (void*)dimension_sizes);
    int messages_available;
    // if this file exists in pwd then terminate
    char sentinel_filename[] = "sentinel";
    while (iteration < max_iterations && !file_exists(sentinel_filename)) {
        start_time = MPI_Wtime();

        // check if a ground station has sent a message
        MPI_Iprobe(MPI_ANY_SOURCE, EVENT_MSG_TAG, MPI_COMM_WORLD,
                   &messages_available, MPI_STATUS_IGNORE);
        while (messages_available) {
            // recv and process ground station messages
            GroundMessage g_msg;
            MPI_Recv(&g_msg, 1, ground_message_type, MPI_ANY_SOURCE,
                     EVENT_MSG_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            time_t epoch_time = (time_t)g_msg.time_since_epoch;
            struct tm* tm = localtime(&epoch_time);
            char display_datetime[64];
            // %c = preferred locale format
            strftime(display_datetime, sizeof(display_datetime), "%c", tm);

            printf("Event from %d [%d] at %s\n", g_msg.rank, g_msg.reading,
                   display_datetime);

            SatelliteReading sr;
            if (compare_satellite_readings(&g_msg, &sr)) {
                printf("Matching reading: %d at (%d,%d), %.9f\n", sr.reading,
                       sr.coords[0], sr.coords[1], sr.mpi_time);
            } else {
                printf("No matching reading\n");
            }

            // keep checking if more messages available
            MPI_Iprobe(MPI_ANY_SOURCE, EVENT_MSG_TAG, MPI_COMM_WORLD,
                       &messages_available, MPI_STATUS_IGNORE);
        }

        printf("[%d] Base|%.9f\n", iteration, start_time);

        sleep_until_interval(start_time, INTERVAL_MILLISECONDS);
        ++iteration;
    }

    if (iteration >= max_iterations) {
        printf("%d iterations reached, terminating\n", iteration);
    } else {
        printf("Sentinel file detected, terminating\n");
    }

    // indicate to thread to terminate
    terminate = 1;
    // broadcast to ground stations to terminate
    MPI_Ibcast(&buf, 1, MPI_CHAR, base_station_world_rank, MPI_COMM_WORLD,
               &bcast_req);
    // since ground stations use Ibcast to receive bcast, must use Ibcast too
    // hence must wait, even though same as normal Bcast
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
            msg.mpi_time = start_time;

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
                MPI_Send(&msg, 1, ground_message_type, base_station_world_rank,
                         EVENT_MSG_TAG, MPI_COMM_WORLD);
            }
        }

        sleep_until_interval(start_time, INTERVAL_MILLISECONDS);
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

int compare_satellite_readings(GroundMessage* g_msg, SatelliteReading* out_sr) {
    SatelliteReading sr;

    size_t i = 0;
    int found_reading = 0;
    size_t i_read_size = sizeof(infrared_readings) / sizeof(*infrared_readings);

    while (i < i_read_size && !found_reading) {
        // copy to a local incase thread overwrites
        sr.reading = infrared_readings[i].reading;
        sr.coords[0] = infrared_readings[i].coords[0];
        sr.coords[1] = infrared_readings[i].coords[1];
        sr.mpi_time = infrared_readings[i].mpi_time;

        int matching_coords = sr.coords[0] == g_msg->coords[0] &&
                              sr.coords[1] == g_msg->coords[1];
        int matching_reading =
            abs(sr.reading - g_msg->reading) <= READING_DIFFERENCE;
        int matching_time = fabs(sr.mpi_time - g_msg->mpi_time) <=
                            (double)MPI_TIME_DIFF_MILLISECONDS / 1000;

        found_reading = matching_coords && matching_reading && matching_time;

        ++i;
    }

    if (found_reading) {
        out_sr->reading = sr.reading;
        out_sr->coords[0] = sr.coords[0];
        out_sr->coords[1] = sr.coords[1];
        out_sr->mpi_time = sr.mpi_time;
    }

    return found_reading;
}

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

void* infrared_thread(void* arg) {
    double start_time;
    int* dimensions = (int*)arg;
    int rows = dimensions[0];
    int cols = dimensions[1];
    size_t i_read_size = sizeof(infrared_readings) / sizeof(*infrared_readings);
    // pointer to next spot to replace in infrared_readings array
    int infrared_readings_latest = 0;

    // prefill the array
    for (size_t i = 0; i < i_read_size; ++i)
        generate_satellite_reading(&infrared_readings[i], rows, cols);

    while (!terminate) {
        // every half interval, generate a new satellite reading
        start_time = MPI_Wtime();

        generate_satellite_reading(
            &infrared_readings[infrared_readings_latest++], rows, cols);
        // point to oldest reading to update (act like queue)
        // will wrap around at end to prevent going beyond bounds
        infrared_readings_latest %= (int)i_read_size;

        sleep_until_interval(start_time, INTERVAL_MILLISECONDS / 2);
    }
    printf("Thread terminating\n");
}

void generate_satellite_reading(SatelliteReading* sr, int rows, int cols) {
    sr->reading = rand() % (1 + MAX_READING_VALUE);
    sr->coords[0] = rand() % rows;
    sr->coords[1] = rand() % cols;
    sr->mpi_time = MPI_Wtime();
}

int file_exists(const char* filename) {
    struct stat b;
    return stat(filename, &b) == 0;
}
