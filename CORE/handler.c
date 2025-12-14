#include "handler.h"

void getPacketType(const void* msg, MessageType* outType)
{
    if (!outType) return; // if null

    *outType = *(MessageType *)msg;
}

void getPacketSize(const MessageType type, const void* msg, uint32_t* outSize)
{
    if (!outSize) return; // if pointer is null
    
    uint32_t size = sizeof(MessageType);

    switch (type)
    {
    case MSG_ACK:
        size += sizeof(uint32_t)*2; // seq + crc32
        break;
    case MSG_HASH:
        size += sizeof(uint32_t) + 32*sizeof(uint8_t); // crc32 + hash
        break;
    case MSG_DATA:
        size += sizeof(uint32_t)*2; // seq + crc32
        size += sizeof(uint16_t);   // data_len
        size += ((DataPacket *)msg)->data_len; // actual data
        break;
    default:
        // unknown type, no payload
        size = 0;
        break;
    }
    *outSize = size;
}

void fill_ACKPacket(uint32_t seq, AckPacket* out)
{
    out->type  = MSG_ACK;
    out->seq   = seq;
    out->crc32 = 0; // important to set to 0 before computing CRC
    out->crc32 = crc32_compute((uint8_t*)out, sizeof(AckPacket) - sizeof(uint32_t));
}

void handle_ACKPacket(const AckPacket* in, DataPacket* out)
{
    out->type  = MSG_DATA;
    out->seq   = in->seq;
    out->crc32 = in->crc32;
    out->data_len = 0;
}

void fill_HashPacket(const HashPacket* in, AckPacket* out)
{
    out->type  = MSG_HASH;
    out->seq   = 0;
    out->crc32 = in->crc32;
}

void handle_HashPacket(const AckPacket* in, HashPacket* out)
{
    out->type  = MSG_HASH;
    out->crc32 = in->crc32;
}

void fill_DataPacket(const AckPacket* in, DataPacket* out)
{
    out->type  = MSG_DATA;
    out->seq   = in->seq;
    out->crc32 = in->crc32;
    out->
}

void handle_DataPacket(const DataPacket* in, AckPacket* out)
{
    out->type  = MSG_ACK;
    out->seq   = in->seq;
    out->crc32 = in->crc32;
}


void handle_message(
    const uint8_t *message_in,
    size_t len,
    uint8_t *message_out,
    size_t *out_len
)
{
    MessageType type;
    size_t size = 0;
    getPacketType(message_in, &type);
    getPacketSize(type, message_in, (uint32_t*)&size);

    verify_crc32(message_in, len, size);

    switch (type) {
    
    case MSG_DATA: {
        const DataPacket *in = (const DataPacket *)message_in;
        AckPacket *out = (AckPacket *)message_out;

        out->type  = MSG_ACK;
        out->seq   = in->seq;
        out->crc32 = in->crc32;

        *out_len = sizeof(AckPacket);
        break;
    }

    case MSG_HASH: {
        const HashPacket *in = (const HashPacket *)message_in;
        AckPacket *out = (AckPacket *)message_out;

        out->type  = MSG_ACK;
        out->seq   = 0;
        out->crc32 = in->crc32;

        *out_len = sizeof(AckPacket);
        break;
    }

    case MSG_ACK:
    default:
        *out_len = 0; // nothing to send
        break;
    }
}
