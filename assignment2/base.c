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
    double prog_start_time = MPI_Wtime();
    double start_time;
    int dimension_sizes[2] = {rows, cols};
    int iteration = 0;
    // doesn't matter what we send in bcast
    char buf = '\0';
    MPI_Request bcast_req;
    pthread_t tid;
    pthread_create(&tid, NULL, infrared_thread, (void*)dimension_sizes);
    int messages_available;
    int true_events = 0, false_events = 0;
    // if this file exists in pwd then terminate
    char sentinel_filename[] = "sentinel";
    // -1 means run forever (until sentinel)
    while ((max_iterations == -1 || iteration < max_iterations) &&
           !file_exists(sentinel_filename)) {
        start_time = MPI_Wtime();

        // check if a ground station has sent a message
        MPI_Iprobe(MPI_ANY_SOURCE, EVENT_MSG_TAG, MPI_COMM_WORLD,
                   &messages_available, MPI_STATUS_IGNORE);
        while (messages_available) {
            // recv and process ground station messages
            GroundMessage g_msg;
            MPI_Recv(&g_msg, 1, ground_message_type, MPI_ANY_SOURCE,
                     EVENT_MSG_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            double recv_time = MPI_Wtime();

            int is_true_alert = process_ground_message(&g_msg, recv_time);
            true_events += 1 & is_true_alert;
            false_events += 1 & !is_true_alert;

            // keep checking if more messages available
            MPI_Iprobe(MPI_ANY_SOURCE, EVENT_MSG_TAG, MPI_COMM_WORLD,
                       &messages_available, MPI_STATUS_IGNORE);
        }

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

    double prog_duration_seconds = MPI_Wtime() - prog_start_time;

    printf(
        "--------------------\nSummary\nSimulation time (seconds): %.5f\nTrue "
        "events: %d\nFalse events: %d\n",
        prog_duration_seconds, true_events, false_events);
}

int process_ground_message(GroundMessage* g_msg, double recv_time) {
    char log_msg[2048];
    int b = 0;

    b += snprintf(log_msg + b, sizeof(log_msg) - b, "--------------------\n");
    b += snprintf(log_msg + b, sizeof(log_msg) - b, "Iteration: %d\n",
                  g_msg->iteration);

    // logged time (when base picked it)
    time_t logged_time = time(NULL);
    struct tm* tm_logged = localtime(&logged_time);
    char display_dt_logged[64];
    // %c = preferred locale format
    strftime(display_dt_logged, sizeof(display_dt_logged), "%c", tm_logged);
    b += snprintf(log_msg + b, sizeof(log_msg) - b, "Logged time: %s\n",
                  display_dt_logged);

    // reported time (from ground)
    time_t reported_time = (time_t)g_msg->time_since_epoch;
    struct tm* tm_reported = localtime(&reported_time);
    char display_dt_reported[64];
    strftime(display_dt_reported, sizeof(display_dt_reported), "%c",
             tm_reported);
    b += snprintf(log_msg + b, sizeof(log_msg) - b, "Reported time: %s\n",
                  display_dt_reported);

    SatelliteReading sr;
    int is_true_alert = compare_satellite_readings(g_msg, &sr);
    if (is_true_alert) {
        b += snprintf(log_msg + b, sizeof(log_msg) - b, "Alert type: True\n\n");
    } else {
        b +=
            snprintf(log_msg + b, sizeof(log_msg) - b, "Alert type: False\n\n");
    }

    char coords_str[10];
    snprintf(coords_str, sizeof(coords_str), "(%d,%d)", g_msg->coords[0],
             g_msg->coords[1]);
    b += snprintf(log_msg + b, sizeof(log_msg) - b, "%-26s %-10s %-10s\n",
                  "Reporting node", "Coords", "Temp");
    b += snprintf(log_msg + b, sizeof(log_msg) - b, "%-26d %-10s %-10d\n\n",
                  g_msg->rank, coords_str, g_msg->reading);

    b += snprintf(log_msg + b, sizeof(log_msg) - b, "%-26s %-10s %-10s\n",
                  "Matching adjacent nodes", "Coords", "Temp");
    for (int i = 0; i < g_msg->matching_neighbours; ++i) {
        char n_coords_str[10];
        snprintf(n_coords_str, sizeof(n_coords_str), "(%d,%d)",
                 g_msg->neighbour_coords[i][0], g_msg->neighbour_coords[i][1]);
        b += snprintf(log_msg + b, sizeof(log_msg) - b, "%-26d %-10s %-10d\n",
                      g_msg->neighbour_ranks[i], n_coords_str,
                      g_msg->neighbour_readings[i]);
    }
    b += snprintf(log_msg + b, sizeof(log_msg) - b, "\n");

    if (is_true_alert) {
        struct tm* tm_satellite = localtime(&(sr.time_since_epoch));
        char display_dt_satellite[64];
        strftime(display_dt_satellite, sizeof(display_dt_satellite), "%c",
                 tm_satellite);
        b += snprintf(log_msg + b, sizeof(log_msg) - b,
                      "Infrared satellite reporting time: %s\n",
                      display_dt_satellite);
        b += snprintf(log_msg + b, sizeof(log_msg) - b,
                      "Infrared satellite reading: %d\n", sr.reading);
        b += snprintf(log_msg + b, sizeof(log_msg) - b,
                      "Infrared satellite reading coords: (%d,%d)\n",
                      sr.coords[0], sr.coords[1]);
    }

    double communication_time = recv_time - g_msg->mpi_time;
    b += snprintf(log_msg + b, sizeof(log_msg) - b,
                  "Communication time (seconds): %.5f\n", communication_time);

    b += snprintf(log_msg + b, sizeof(log_msg) - b, "--------------------\n");
    printf("%s", log_msg);

    return is_true_alert;
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
        sr.time_since_epoch = infrared_readings[i].time_since_epoch;

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
        out_sr->time_since_epoch = sr.time_since_epoch;
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
    return arg;
}

void generate_satellite_reading(SatelliteReading* sr, int rows, int cols) {
    sr->reading = rand() % (1 + MAX_READING_VALUE);
    sr->coords[0] = rand() % rows;
    sr->coords[1] = rand() % cols;
    sr->mpi_time = MPI_Wtime();
    sr->time_since_epoch = time(NULL);
}

int file_exists(const char* filename) {
    struct stat b;
    return stat(filename, &b) == 0;
}