// protocol.h
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define PACKET_MAX_SIZE 1024
#define DATA_MAX_SIZE   (PACKET_MAX_SIZE - 10) // BUFFER_LEN

#ifdef SENDER
#define TARGET_PORT 5555
#define LOCAL_PORT 7777
#endif // SENDER

#ifdef RECEIVER // Client
#define TARGET_PORT 7777
#define LOCAL_PORT 5555
#endif // RECEIVER

typedef enum {
    MSG_ACK,
    MSG_DATA,
    MSG_HASH,
    MSG_NUM,

} MessageType;

typedef enum {
    VALIDITY_OK = 1,
    VALIDITY_ERROR = 0,
    
} ValidityType;


typedef struct {
    MessageType type;      // typ zprávy
    uint32_t seq;        // číslo paketu

    uint16_t data_len;   // délka dat
    uint8_t  data[DATA_MAX_SIZE];

    uint32_t crc32;      // CRC celého data pole
} DataPacket;

typedef struct {
    MessageType type;      // typ zprávy

    uint8_t  hash[32];   // SHA256 hash

    uint32_t crc32;      // CRC celého data pole
} HashPacket;

typedef struct {
    MessageType type;      // typ zprávy

    uint32_t seq;        // číslo paketu

    uint8_t valid;     // 1 = OK, 0 = chyba

    uint32_t crc32;      // CRC celého data pole

} AckPacket;

#endif
