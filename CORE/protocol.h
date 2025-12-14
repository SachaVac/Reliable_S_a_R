// protocol.h
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define PACKET_MAX_SIZE 1024
#define DATA_MAX_SIZE   (PACKET_MAX_SIZE - 10)

// Jednoduchý datový paket pro Stop-and-Wait
typedef struct {
    uint32_t seq;        // číslo paketu
    uint32_t crc32;      // CRC celého data pole
    uint16_t data_len;   // délka dat
    uint8_t  data[DATA_MAX_SIZE];
} DataPacket;

#endif
