// protocol.h
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define LOCAL
//#define STOP_AND_WAIT

#define PACKET_MAX_SIZE 1024
#define DATA_MAX_SIZE   (PACKET_MAX_SIZE - 10) // BUFFER_LEN

#ifdef SENDER
#define TARGET_PORT 15000//
#define LOCAL_PORT 14001//
#endif // SENDER

#ifdef RECEIVER // Client
#define TARGET_PORT 14001
#define LOCAL_PORT 15000
#endif // RECEIVER

typedef enum {
    MSG_ACK,
    MSG_DATA,
    MSG_HASH,
    MSG_NUM,

} MessageType;

typedef struct {
    MessageType type;      // typ zprávy
    uint32_t seq;        // číslo paketu
    uint32_t crc32;      // CRC celého data pole

    uint16_t data_len;   // délka dat
    uint8_t  data[DATA_MAX_SIZE];
} DataPacket;

typedef struct {
    MessageType type;      // typ zprávy
    uint32_t crc32;      // CRC celého data pole
} HashPacket;


typedef struct {
    MessageType type;      // typ zprávy
    uint32_t seq;        // číslo paketu
    uint32_t crc32;      // CRC celého data pole
} AckPacket;

#endif
