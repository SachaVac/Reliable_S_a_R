// protocol.h
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define PACKET_MAX_SIZE 1024
#define DATA_MAX_SIZE   (PACKET_MAX_SIZE - 10)

#ifdef SENDER
#define TARGET_PORT 5555
#define LOCAL_PORT 7777
#endif // SENDER

#ifdef RECEIVER // Client
#define TARGET_PORT 7777
#define LOCAL_PORT 5555
#endif // RECEIVER

#define MAX_PACKET_SIZE 1024
#define BUFFERS_LEN 1000 // 512
#define MAX_SEGMENTS 4096*32
#define MAX_FILE_NAMES 256 // it is not length of file
#define MAX_FILE_NAME_LENGTH 256

#define MAX_RETRIES 0
#define RETRY_TIMEOUT_MS 1000  //ish timeout between retries



// Jednoduchý datový paket pro Stop-and-Wait
typedef struct {
    uint32_t seq;        // číslo paketu
    uint32_t crc32;      // CRC celého data pole
    uint16_t data_len;   // délka dat
    uint8_t  data[DATA_MAX_SIZE];
} DataPacket;

#endif
