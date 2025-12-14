// messages.h (UDP)
/*
RequestFileNames
-> AckFileNamesSegment (with count-1 and first name)
if the count is >1 send the
RequestTransferName (1)
-> AckFileNamesSegment (with count-2 and second name)
...
-> AckFileNamesSegment (0 and last name)
---------------
I have all names (select name)
RequestFile (Name)
-> AckDataSegment (count-1 and first segment data)
if the count is >1 send the
RequestTransferFile (1)
-> AckDataSegment (count-2 and second segment data)
...
-> AckDataSegment (0 and last segment data)

---------------
RequestCloseConnection
-> Ack
*/

#pragma once

#include <cstdint> // For fixed-width integers like uint16_t
#include <winsock2.h>


// --- CONSTANTS ---
#define SENDER // Server

#ifdef SENDER
#define TARGET_PORT 5555
#define LOCAL_PORT 7777
#endif // SENDER

#ifdef RECEIVER // Client
#define TARGET_PORT 7777
#define LOCAL_PORT 5555
#endif // RECEIVER

#define MAX_PACKET_SIZE 1024
#define BUFFERS_LEN 512
#define MAX_SEGMENTS 4096*32
#define MAX_FILE_NAMES 256 // it is not length of file
#define MAX_FILE_NAME_LENGTH 256
#define MAX_RETRIES 30
#define TIMEOUT_MS 1.5 // in ms


// --- MESSAGE TYPES (Packet IDs) ---
enum MessageType : uint16_t {

    // --- Server messages ---
    MSG_ACK_FILE_NAMES = 0,     // Sends list of available files. Provides total number of segments to be sent. Each segment includes one name of file.
    MSG_ACK_FILE_DATA,      // Acknowledges a file request. Provides total number of segments to be sent. Each segment includes 1024 bits of file.
    MSG_ACK_HASH_DATA,      // hash response
    MSG_ACK,                // Acknowledges receipt of a message (close connection).

    // --- Client messages ---
    MSG_REQUEST_FILE_NAMES,         // Requests the list of available files (no payload, acts like a ping). 
    MSG_REQUEST_TRANSFER_NAME,      // Requests a specific name segment by sequence number.
    MSG_REQUEST_FILE,               // Requests a specific file by name. Server replies with total segments.
    MSG_REQUEST_TRANSFER_FILE,      // Requests a specific file segment by sequence number.
    MSG_REQUEST_HASH_FILE,          // sends hash
    MSG_CLOSE_CONNECTION,           // Signals intent to close the connection.

    MSG_NUM,                        // Total number of message types.
};

// --- STRUCTS (Packet Structures) ---
// --- Base message header ---
struct MessageHeader {
    uint16_t type;        // MessageType
    uint32_t sequenceNum; // Sequence number
    uint32_t checksum;    // Optional checksum - TODO implement CRC 32 bit 
};

// --- Unified message container ---
struct Message {
    MessageHeader header;
    const void* payload;         // Pointer to one of the structs above
};

// --- Server responses --- 

// Each segment contains one filename
struct AckFileNamesSegment {
    uint16_t remaining_count;    // Total number of name segments
    char name[256];              // Null-terminated filename (one per segment)
};

// Each segment contains one filename
struct AckFileDataSegment {
    uint32_t remaining_count; // segments left
    uint16_t data_len;        // valid bytes in this segment
    uint8_t data[BUFFERS_LEN]; // actual buffer
};

struct AckFileHash {
    uint8_t hash[16];   // MD5 = 16 B
};

// Generic ACK
struct Ack {};

// Client requests

// Requests list of all files
struct RequestFileNames {};

struct RequestTransferName {
    uint32_t seqNum;             // Sequence of name segment requested
};

struct RequestFile {
    char fileName[256];          // Null-terminated requested file
};

struct RequestTransferFile {
    uint32_t seqNum;             // Sequence of file segment requested
};

// Close connection message
struct RequestCloseConnection {};


