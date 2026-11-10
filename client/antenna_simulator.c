#include <stdlib.h> 
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/resource.h>
#include <string.h>
#include "antenna_simulator.h"

struct timespec delay = {
    .tv_sec = 0,
    .tv_nsec = 100000000L  // 100 ms
};

int antenna_packet_init(struct antenna_packet *packet, uint8_t id,
                        uint16_t sequence_number, const uint16_t *samples,
                        uint16_t sample_count)
{
    if (packet == NULL || sample_count > MAX_SAMPLES_PER_PACKET ||
        (sample_count > 0 && samples == NULL))
    {
        return -1;
    }

    memset(packet, 0, sizeof(*packet));
    packet->header.magic_number = ANTENNA_PROTOCOL_MAGIC;
    packet->header.version = ANTENNA_PROTOCOL_VERSION;
    packet->header.id = id;
    packet->header.sequence_number = sequence_number;
    packet->header.payload_length = sample_count;
    if (sample_count > 0)
    {
        memcpy(packet->samples, samples, sample_count * sizeof(*samples));
    }

    return 0;
}

int generate_packet(struct antenna_packet *packet, uint8_t id,
                    uint16_t *sequence_number)
{
    if (packet == NULL || sequence_number == NULL)
    {
        return -1;
    }

    uint16_t samples_count = (rand() % MAX_SAMPLES_PER_PACKET) + 1;
    uint16_t samples[samples_count];
    for (int i = 0; i < samples_count; i++)
    {
        samples[i] = (uint16_t)((rand() % 65536) - 32768);
    }

    if (antenna_packet_init(packet, id, *sequence_number, samples,
                            samples_count) != 0)
    {
        return -1;
    }

    (*sequence_number)++;
    return 0;
}
  