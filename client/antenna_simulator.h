#ifndef ANTENNA_SIMULATOR_H
#define ANTENNA_SIMULATOR_H

#include <arpa/inet.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h> 

#define MAX_SAMPLES_PER_PACKET 50
#define ANTENNA_PROTOCOL_MAGIC 0x414E5445u
#define ANTENNA_PROTOCOL_VERSION 1u

#define CHECK(x, y) do { if((x) == -1) { perror(y); return -1;}} while (0);

extern struct timespec delay;

struct __attribute__((packed)) antenna_packet_header {
    uint32_t magic_number;
    uint8_t  version;
    uint8_t  id;
    uint16_t sequence_number;
    uint16_t payload_length;
};

struct __attribute__((packed)) antenna_packet {
    struct antenna_packet_header header;
    uint16_t samples[MAX_SAMPLES_PER_PACKET];
    uint16_t crc;
};


int antenna_packet_init(struct antenna_packet *packet, uint8_t id,
                        uint16_t sequence_number, const uint16_t *samples,
                        uint16_t sample_count);

int generate_packet(struct antenna_packet *packet, uint8_t id,
                    uint16_t *sequence_number);

static inline size_t antenna_packet_payload_bytes(uint16_t payload_length)
{
    return (size_t)payload_length * sizeof(uint16_t);
}

static inline size_t antenna_packet_total_size(uint16_t payload_length)
{
    return sizeof(struct antenna_packet_header) +
           antenna_packet_payload_bytes(payload_length) +
           sizeof(uint16_t);
}

// CRC-16/CCITT-FALSE
static inline uint16_t crc16_ccitt_false(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < length; i++)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; bit++)
        {
            if (crc & 0x8000)
            {
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            }
            else
            {
                crc = (uint16_t)(crc << 1);
            }
        }
    }

    return crc;
}

#endif