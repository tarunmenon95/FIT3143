#include "base.h"

#include <math.h>
#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

#include "common.h"

// global arr for thread to store its satellite readings
SatelliteReading infrared_readings[30];

// flag to indicate whether thread should terminate
int terminate = 0;

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
    // since ground stations use Ibcast to receive bcast, must use Ibcast here
    MPI_Ibcast(&buf, 1, MPI_CHAR, base_station_world_rank, MPI_COMM_WORLD,
               &bcast_req);
    // hence must wait, even though essentially same as normal Bcast
    MPI_Wait(&bcast_req, MPI_STATUS_IGNORE);
    pthread_join(tid, NULL);
    printf("Base station exiting\n");
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
    return arg;
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
