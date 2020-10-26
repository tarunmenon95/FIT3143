#include "base.h"

#include <math.h>
#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "common.h"

// global arr for thread to store its satellite readings
SatelliteReading infrared_readings[30];

// flag to indicate whether thread should terminate
int terminate = 0;

void base_station(int base_station_world_rank, int max_iterations, int rows,
                  int cols, MPI_Datatype ground_message_type,
                  double mpi_start_wtime) {
    FILE* log_fp = fopen("base_station.log", "w");
    char init_msg[128];
    char init_msg_dt[64];
    format_to_datetime(time(NULL), init_msg_dt, sizeof(init_msg_dt));
    snprintf(init_msg, sizeof(init_msg),
             "Start time: %s\nGrid size: %d rows, %d columns\n\n", init_msg_dt,
             rows, cols);
    printf("%s", init_msg);
    fprintf(log_fp, "%s", init_msg);

    double start_time;
    // doesn't matter what we send in bcast
    char buf = '\0';
    MPI_Request bcast_req;
    SatelliteThreadArgs t_args;
    t_args.rows = rows;
    t_args.cols = cols;
    t_args.mpi_start_wtime = mpi_start_wtime;
    pthread_t tid;
    pthread_create(&tid, NULL, infrared_thread, (void*)&t_args);
    int messages_available;
    int iteration = 0, true_events = 0, false_events = 0;
    // if this file exists in pwd then terminate
    char sentinel_filename[] = "sentinel";
    // -1 means run forever (until sentinel)
    while ((max_iterations == -1 || iteration < max_iterations) &&
           !file_exists(sentinel_filename)) {
        start_time = MPI_Wtime() - mpi_start_wtime;

        // check if a ground station has sent a message
        MPI_Iprobe(MPI_ANY_SOURCE, EVENT_MSG_TAG, MPI_COMM_WORLD,
                   &messages_available, MPI_STATUS_IGNORE);
        while (messages_available) {
            // recv and process ground station messages
            GroundMessage g_msg;
            MPI_Recv(&g_msg, 1, ground_message_type, MPI_ANY_SOURCE,
                     EVENT_MSG_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            double recv_time = MPI_Wtime() - mpi_start_wtime;

            int is_true_alert =
                process_ground_message(log_fp, &g_msg, recv_time);
            true_events += 1 & is_true_alert;
            false_events += 1 & !is_true_alert;

            // keep checking if more messages available
            MPI_Iprobe(MPI_ANY_SOURCE, EVENT_MSG_TAG, MPI_COMM_WORLD,
                       &messages_available, MPI_STATUS_IGNORE);
        }

        sleep_until_interval(start_time, INTERVAL_MILLISECONDS,
                             mpi_start_wtime);
        ++iteration;
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

    double prog_duration_seconds = MPI_Wtime() - mpi_start_wtime;

    char end_msg[256];
    int end_msg_len = 0;
    if (!file_exists(sentinel_filename)) {
        end_msg_len +=
            snprintf(end_msg, sizeof(end_msg),
                     "\n%d iterations reached, terminating\n", iteration);
    } else {
        end_msg_len += snprintf(end_msg, sizeof(end_msg),
                                "\nSentinel file detected, terminating\n");
    }
    snprintf(end_msg + end_msg_len, sizeof(end_msg) - end_msg_len,
             "--------------------\nSummary:\n\nSimulation time (seconds): "
             "%.5f\nTrue events: %d\nFalse events: %d\n",
             prog_duration_seconds, true_events, false_events);
    printf("%s", end_msg);
    fprintf(log_fp, "%s", end_msg);

    fclose(log_fp);
}

int process_ground_message(FILE* log_fp, GroundMessage* g_msg,
                           double recv_time) {
    char log_msg[2048];
    int b = 0;

    b += snprintf(log_msg + b, sizeof(log_msg) - b, "--------------------\n");
    b += snprintf(log_msg + b, sizeof(log_msg) - b, "Iteration: %d\n",
                  g_msg->iteration);

    // logged time (when base picked it)
    char display_dt_logged[64];
    format_to_datetime(time(NULL), display_dt_logged,
                       sizeof(display_dt_logged));
    b += snprintf(log_msg + b, sizeof(log_msg) - b, "Logged time: %s\n",
                  display_dt_logged);

    // reported time (from ground)
    char display_dt_reported[64];
    format_to_datetime(g_msg->time_since_epoch, display_dt_reported,
                       sizeof(display_dt_reported));
    b += snprintf(log_msg + b, sizeof(log_msg) - b, "Reported time: %s\n",
                  display_dt_reported);

    // true or false event
    SatelliteReading sr;
    int is_true_alert = compare_satellite_readings(g_msg, &sr);
    if (is_true_alert)
        b += snprintf(log_msg + b, sizeof(log_msg) - b, "Alert type: True\n\n");
    else
        b +=
            snprintf(log_msg + b, sizeof(log_msg) - b, "Alert type: False\n\n");

    // print details of reporting station
    char coords_str[10];
    char ip_str[20];
    char mac_str[20];
    snprintf(coords_str, sizeof(coords_str), "(%d,%d)", g_msg->coords[0],
             g_msg->coords[1]);
    format_ip_addr(g_msg->ip_addr, ip_str);
    format_mac_addr(g_msg->mac_addr, mac_str);
    b += snprintf(log_msg + b, sizeof(log_msg) - b,
                  "%-26s %-10s %-10s %-20s %-20s\n", "Reporting node", "Coords",
                  "Temp", "IP Address", "MAC Address");
    b += snprintf(log_msg + b, sizeof(log_msg) - b,
                  "%-26d %-10s %-10d %-20s %-20s\n\n", g_msg->rank, coords_str,
                  g_msg->reading, ip_str, mac_str);

    b += snprintf(log_msg + b, sizeof(log_msg) - b,
                  "%-26s %-10s %-10s %-20s %-20s\n", "Matching adjacent nodes",
                  "Coords", "Temp", "IP Address", "MAC Address");
    // print details of neighbours to the reporting station
    for (int i = 0; i < g_msg->matching_neighbours; ++i) {
        format_ip_addr(g_msg->neighbour_ip_addrs[i], ip_str);
        format_mac_addr(g_msg->neighbour_mac_addrs[i], mac_str);
        snprintf(coords_str, sizeof(coords_str), "(%d,%d)",
                 g_msg->neighbour_coords[i][0], g_msg->neighbour_coords[i][1]);
        b += snprintf(log_msg + b, sizeof(log_msg) - b,
                      "%-26d %-10s %-10d %-20s %-20s\n",
                      g_msg->neighbour_ranks[i], coords_str,
                      g_msg->neighbour_readings[i], ip_str, mac_str);
    }
    b += snprintf(log_msg + b, sizeof(log_msg) - b, "\n");

    // if true alert then also print satellite reading
    if (is_true_alert) {
        char display_dt_satellite[64];
        format_to_datetime(sr.time_since_epoch, display_dt_satellite,
                           sizeof(display_dt_satellite));
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
    fprintf(log_fp, "%s", log_msg);

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
        memcpy(sr.coords, infrared_readings[i].coords, 2 * sizeof(int));
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
        memcpy(out_sr->coords, sr.coords, 2 * sizeof(int));
        out_sr->mpi_time = sr.mpi_time;
        out_sr->time_since_epoch = sr.time_since_epoch;
    }

    return found_reading;
}

void* infrared_thread(void* arg) {
    double start_time;
    SatelliteThreadArgs* t_args = (SatelliteThreadArgs*)arg;
    int rows = t_args->rows;
    int cols = t_args->cols;
    double mpi_start_wtime = t_args->mpi_start_wtime;
    size_t i_read_size = sizeof(infrared_readings) / sizeof(*infrared_readings);
    // pointer to next spot to replace in infrared_readings array
    int infrared_readings_latest = 0;

    // prefill the array
    for (size_t i = 0; i < i_read_size; ++i)
        generate_satellite_reading(&infrared_readings[i], rows, cols,
                                   mpi_start_wtime);

    while (!terminate) {
        // every half interval, generate a new satellite reading
        start_time = MPI_Wtime() - mpi_start_wtime;

        generate_satellite_reading(
            &infrared_readings[infrared_readings_latest++], rows, cols,
            mpi_start_wtime);
        // point to oldest reading to update (act like queue)
        // will wrap around at end to prevent going beyond bounds
        infrared_readings_latest %= (int)i_read_size;

        sleep_until_interval(start_time, INTERVAL_MILLISECONDS / 2,
                             mpi_start_wtime);
    }
    return arg;
}

void generate_satellite_reading(SatelliteReading* sr, int rows, int cols,
                                double mpi_start_wtime) {
    sr->reading = rand() % (1 + MAX_READING_VALUE);
    sr->coords[0] = rand() % rows;
    sr->coords[1] = rand() % cols;
    sr->mpi_time = MPI_Wtime() - mpi_start_wtime;
    sr->time_since_epoch = time(NULL);
}

int file_exists(const char* filename) {
    struct stat b;
    return stat(filename, &b) == 0;
}

int format_to_datetime(time_t t, char* out_buf, size_t out_buf_len) {
    struct tm* tm = localtime(&t);
    return strftime(out_buf, out_buf_len, "%c", tm);
}
