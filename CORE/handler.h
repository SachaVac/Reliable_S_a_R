#ifndef HANDLER_H
#define HANDLER_H
#include "protocol.h"
#include "crc32.h"
#include "sha256.h"
#include <stdio.h>

void open_file(const char* filename); //run at start

void getPacketType(const void* msg, MessageType* outType);
void getPacketSize(const MessageType type, const void* msg, uint32_t* outSize);

void handle_message(
    const uint8_t *message_in,
    size_t len,
    uint8_t *message_out,
    size_t *out_len
);


#endif