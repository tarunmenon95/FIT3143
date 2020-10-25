#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED

#include <mpi.h>
#include <stdint.h>

// can vary these
#define INTERVAL_MILLISECONDS 200
// what constitutes an event reading
#define READING_THRESHOLD 80  // 80
// allowable absolute diff b/w readings
#define READING_DIFFERENCE 25  // 5
// allowable absolute diff b/w mpi times for event & thread reading
#define MPI_TIME_DIFF_MILLISECONDS 100
#define MAX_READING_VALUE 100
// don't vary these
#define SECONDS_TO_NANOSECONDS 1000000000
#define EVENT_MSG_TAG 0

void create_ground_message_type(MPI_Datatype*);
void sleep_until_interval(double, int, double);
int get_device_addresses(unsigned char[4], unsigned char[6]);
void format_ip_addr(unsigned char[4], char*);
void format_mac_addr(unsigned char[6], char*);

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
    unsigned char ip_addr[4];
    unsigned char neighbour_ip_addrs[4][4];
    unsigned char mac_addr[6];
    unsigned char neighbour_mac_addrs[4][6];
} GroundMessage;

#endif
