#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED

#include <mpi.h>

// can vary these
#define INTERVAL_MILLISECONDS 500
// what constitutes an event reading
#define READING_THRESHOLD 80  // 80
// allowable absolute diff b/w readings
#define READING_DIFFERENCE 5  // 5
// allowable absolute diff b/w mpi times for event & thread reading
#define MPI_TIME_DIFF_MILLISECONDS 200
#define MAX_READING_VALUE 100
// don't vary these
#define SECONDS_TO_NANOSECONDS 1000000000
#define EVENT_MSG_TAG 0

void create_ground_message_type(MPI_Datatype*);
void sleep_until_interval(double, int);

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

#endif
