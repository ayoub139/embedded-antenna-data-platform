#include <stdlib.h> 
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/resource.h>
#include "antenna_simulator.h"

struct timespec delay = {
    .tv_sec = 0,
    .tv_nsec = 100000000L  // 100 ms
};

void generate_packet(struct antenna_packet * packet, int id, int* seq_nbr)
{
    packet->id = id;
    packet->sequence_number = (*seq_nbr)++;
    for(int i = 0 ; i < MAX_SAMPLES_PER_PACKET; i++)
    {
        packet->samples[i] = (int16_t)(rand() % 65536 - 32768);
    } 
}
  