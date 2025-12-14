#include "handler.h"

void fill_ACKPacket(uint32_t seq, AckPacket* out);
uint8_t handle_ACKPacket(AckPacket* in, uint32_t *seq);


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

void fill_ACKPacket(uint32_t seq, AckPacket* out, ValidityType validity)
{
    out->type  = MSG_ACK;
    out->seq   = seq;
    out->valid = validity;
    out->crc32 = 0; // important to set to 0 before computing CRC
    out->crc32 = crc32_compute((uint8_t*)out, sizeof(AckPacket) - sizeof(uint32_t));
}

uint8_t handle_ACKPacket(AckPacket* in, uint32_t *seq)
{
    *seq = in->seq;
    if (in->valid == VALIDITY_OK)
        return 1;
    else if (in->valid == VALIDITY_ERROR)
    {
        return 0;
    }

    return 0; // unknown validity
}

void fill_HashPacket(const HashPacket* in, AckPacket* out)
{
    out->type  = MSG_HASH;
    out->seq   = 0;
    out->crc32 = in->crc32;
}

void handle_HashPacket(const AckPacket* in)
{
    out->crc32 = in->crc32;
}

void fill_DataPacket(const AckPacket* in, DataPacket* out)
{
    out->type  = MSG_DATA;
    out->seq   = in->seq;
    out->crc32 = in->crc32;
    out->
}

void handle_DataPacket(const DataPacket* in)
{
    out->type  = MSG_ACK;
    out->seq   = in->seq;
    out->crc32 = in->crc32;
}


uint8_t handle_message(
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

    uint8_t val = verify_crc32(message_in, len, size); // if CRC is valid -> val == 1

    switch (type) {
    
    case MSG_DATA: {
        const DataPacket *in = (const DataPacket *)message_in;
        AckPacket *out = (AckPacket *)message_out;

        handle_DataPacket(in);

        if (val == 1)
            fill_ACKPacket(in->seq, out, VALIDITY_OK);
        else
            fill_ACKPacket(in->seq, out, VALIDITY_ERROR);

        *out_len = sizeof(AckPacket);
        break;
    }

    case MSG_HASH: {
        const HashPacket *in = (const HashPacket *)message_in;
        AckPacket *out = (AckPacket *)message_out;

        handle_HashPacket(in);

        if (val == 1)
            fill_ACKPacket(0, out, VALIDITY_OK);
        else
            fill_ACKPacket(0, out, VALIDITY_ERROR);

        *out_len = sizeof(AckPacket);
        break;
    }

    case MSG_ACK:
        handle_ACKPacket((AckPacket *)message_in, NULL); // TODO

    default:
        *out_len = 0; // nothing to send
        break;
    }
}
