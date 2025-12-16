// protocol.h
#ifndef PROTOCOL_H
#define PROTOCOL_H

//#define STOP_AND_WAIT

#include <stdint.h>

#define PACKET_MAX_SIZE 1024
#define DATA_MAX_SIZE   (PACKET_MAX_SIZE - 10) // BUFFER_LEN

// window config

#define MAX_PACKETS  1024

#ifdef SENDER
#define TARGET_PORT 15000//
#define LOCAL_PORT 14000//
#endif // SENDER

#ifdef RECEIVER // Client
#define TARGET_PORT 14000
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


// --------- window structure (for queue) -------------
/*typedef struct {
    DataPacket pkt;
    int acked;
    struct timeval sent_time;
} WindowSlot;*/

//uint32_t base = 0;      // nejstarší nepotvrzený seq
//uint32_t next_seq = 0; // další seq k odeslání
//WindowSlot window[WINDOW_SIZE];


// Struktura Slot je rozšířena, aby sledovala, zda byl paket potvrzen.
typedef struct {
    int acked;      // 1, pokud byl paket potvrzen; 0, pokud ne
    struct timeval sent; // Čas odeslání/retransmise (časovač pro KAŽDÝ paket)
} Slot;
#endif
