#ifndef BASE_H_INCLUDED
#define BASE_H_INCLUDED

#include <mpi.h>
#include <time.h>

#include "common.h"

typedef struct {
    int coords[2];
    int reading;
    double mpi_time;
    time_t time_since_epoch;
} SatelliteReading;

void base_station(int, int, int, int, MPI_Datatype);
void* infrared_thread(void*);
void generate_satellite_reading(SatelliteReading*, int, int);
int file_exists(const char*);
int compare_satellite_readings(GroundMessage*, SatelliteReading*);
int process_ground_message(GroundMessage*, double);

#endif
