#ifndef MESSAGES_H
#define MESSAGES_H

#include <cstdint>
#include <cstring>

#pragma pack(push, 1)

// =====================================================
// PROTOCOL MESSAGE TYPES
// =====================================================

#define MSG_ACK_FILE_NAMES      0
#define MSG_ACK_FILE_DATA       1
#define MSG_ACK_HASH_DATA       2
#define MSG_ACK                 3

#define MSG_REQUEST_FILE_NAMES      4
#define MSG_REQUEST_TRANSFER_NAME   5
#define MSG_REQUEST_FILE            6
#define MSG_REQUEST_TRANSFER_FILE   7
#define MSG_REQUEST_HASH_FILE       8
#define MSG_CLOSE_CONNECTION        9

// =====================================================
// PROTOCOL CONSTANTS
// =====================================================

#define BUFFERS_LEN                 512
#define MAX_FILE_NAMES              256
#define MAX_SEGMENTS                65536
#define MAX_PACKET_SIZE             1024

// =====================================================
// MESSAGE HEADER (fixed 12 bytes)
// =====================================================

struct MessageHeader {
    uint16_t type;          // Message type (0-9)
    uint16_t _padding;      // Alignment padding (2 bytes)
    uint32_t sequenceNum;   // Sequence number
    uint32_t checksum;      // CRC-32 checksum (Ethernet/ZIP polynomial)
};

// =====================================================
// PAYLOAD STRUCTURES
// =====================================================

struct AckFileNamesSegment {
    uint16_t remaining_count;  // How many names remain (including this one)
    char name[256];            // File name (null-terminated)
};

struct AckFileDataSegment {
    uint32_t remaining_count;  // How many segments remain (including this one)
    uint16_t data_len;         // Actual bytes in this segment
    uint8_t data[BUFFERS_LEN]; // File data
};

struct AckFileHash {
    uint8_t hash[16];          // MD5 hash (16 bytes)
};

struct Ack {
    // Empty payload
};

// =====================================================
// UNION MESSAGE PAYLOAD
// =====================================================

union MessagePayload {
    AckFileNamesSegment fileNamesSegment;
    AckFileDataSegment fileDataSegment;
    AckFileHash fileHash;
    Ack ack;
};

struct Message {
    MessageHeader header;
    MessagePayload payload;
};

#pragma pack(pop)

#endif // MESSAGES_H
