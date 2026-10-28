#ifndef ANTENNA_SIMULATOR_H
#define ANTENNA_SIMULATOR_H

#include <stdint.h>
#include <time.h> 

#define MAX_SAMPLES_PER_PACKET 16

#define CHECK(x, y) do { if((x) == -1) { perror(y); return -1;}} while (0);

extern struct timespec delay;


struct antenna_packet {
    int id;
    int sequence_number;
    int timestamp;
    int16_t samples[MAX_SAMPLES_PER_PACKET];
};

void generate_packet(struct antenna_packet * packet, int id, int* seq_nbr);

#endif