#include "common.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <linux/if.h>
#include <mpi.h>
#include <netpacket/packet.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

void create_ground_message_type(MPI_Datatype *ground_message_type) {
    // create MPI datatype for GroundMessage
    int no_of_fields = 14;
    int block_lengths[14] = {1, 1, 1, 1, 2,     4, 4 * 2,
                             4, 1, 1, 4, 4 * 4, 6, 4 * 6};
    MPI_Aint displacements[14];
    displacements[0] = offsetof(GroundMessage, iteration);
    displacements[1] = offsetof(GroundMessage, reading);
    displacements[2] = offsetof(GroundMessage, rank);
    displacements[3] = offsetof(GroundMessage, matching_neighbours);
    displacements[4] = offsetof(GroundMessage, coords);
    displacements[5] = offsetof(GroundMessage, neighbour_ranks);
    displacements[6] = offsetof(GroundMessage, neighbour_coords);
    displacements[7] = offsetof(GroundMessage, neighbour_readings);
    displacements[8] = offsetof(GroundMessage, mpi_time);
    displacements[9] = offsetof(GroundMessage, time_since_epoch);
    displacements[10] = offsetof(GroundMessage, ip_addr);
    displacements[11] = offsetof(GroundMessage, neighbour_ip_addrs);
    displacements[12] = offsetof(GroundMessage, mac_addr);
    displacements[13] = offsetof(GroundMessage, neighbour_mac_addrs);
    MPI_Datatype datatypes[14] = {
        MPI_INT,           MPI_INT,           MPI_INT,
        MPI_INT,           MPI_INT,           MPI_INT,
        MPI_INT,           MPI_INT,           MPI_DOUBLE,
        MPI_LONG,          MPI_UNSIGNED_CHAR, MPI_UNSIGNED_CHAR,
        MPI_UNSIGNED_CHAR, MPI_UNSIGNED_CHAR};
    MPI_Type_create_struct(no_of_fields, block_lengths, displacements,
                           datatypes, ground_message_type);
    MPI_Type_commit(ground_message_type);
}

void sleep_until_interval(double start_time, int interval_ms,
                          double mpi_start_wtime) {
    // sleep until interval_ms has passed since start_time
    struct timespec ts;
    double end_time = MPI_Wtime() - mpi_start_wtime;
    double sleep_length =
        (start_time + ((double)interval_ms / 1000) - end_time) *
        SECONDS_TO_NANOSECONDS;
    ts.tv_sec = 0;
    ts.tv_nsec = (long)sleep_length;
    nanosleep(&ts, NULL);
}

int get_device_addresses(unsigned char ip_addr[4], unsigned char mac_addr[6]) {
    // MAC: https://stackoverflow.com/a/35242525
    // IP: https://stackoverflow.com/a/4139893
    struct ifaddrs *ifaddr, *ifa;
    struct sockaddr_ll *mac_a;
    struct sockaddr_in *ip_a;
    unsigned char *tmp_ip_addr;
    char ip_flg = 0, mac_flg = 0;
    char *preferred_names[] = {"eth0", "enp0s3"};
    size_t preferred_names_len =
        sizeof(preferred_names) / sizeof(*preferred_names);

    // get linked list of network interfaces
    if (getifaddrs(&ifaddr) == -1) return 0;

    // traverse linked list
    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        // has to have an address, and not be the loopback device
        if (!(ifa->ifa_addr) || (ifa->ifa_flags & IFF_LOOPBACK)) continue;

        // only want certain network interfaces
        int matching_name = 0;
        for (size_t i = 0; i < preferred_names_len; ++i)
            if (!strcmp(ifa->ifa_name, preferred_names[i])) matching_name = 1;
        if (!matching_name) continue;

        switch (ifa->ifa_addr->sa_family) {
            case AF_PACKET:  // MAC address
                mac_a = (struct sockaddr_ll *)ifa->ifa_addr;
                if (mac_a->sll_halen != 6) break;
                for (int i = 0; i < 6; ++i) mac_addr[i] = mac_a->sll_addr[i];
                mac_flg = 1;
                break;
            case AF_INET:  // IP address
                ip_a = (struct sockaddr_in *)ifa->ifa_addr;
                tmp_ip_addr = (unsigned char *)&ip_a->sin_addr.s_addr;
                for (int i = 0; i < 4; ++i) ip_addr[i] = tmp_ip_addr[i];
                ip_flg = 1;
                break;
        }
    }
    freeifaddrs(ifaddr);
    return ip_flg && mac_flg;
}

void format_ip_addr(unsigned char ip_addr[4], char *out_str) {
    sprintf(out_str, "%d.%d.%d.%d", ip_addr[0], ip_addr[1], ip_addr[2],
            ip_addr[3]);
}

void format_mac_addr(unsigned char mac_addr[6], char *out_str) {
    sprintf(out_str, "%02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0], mac_addr[1],
            mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
}
