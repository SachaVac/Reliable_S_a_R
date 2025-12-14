#include "crc32.h"

static uint32_t table[256];
static int table_computed = 0;

static void make_table(void)
{
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        table[i] = c;
    }
    table_computed = 1;
}

uint32_t crc32_compute(const uint8_t *data, size_t len)
{
    uint32_t c = 0xffffffff;

    if (!table_computed)
        make_table();

    for (size_t i = 0; i < len; i++)
        c = table[(c ^ data[i]) & 0xff] ^ (c >> 8);

    return c ^ 0xffffffff;
}

uint8_t crc32_verify(const uint8_t *data, size_t len)
{
    // CRC is stored in the last 4 bytes
    size_t data_len = len - sizeof(uint32_t); //len - size of CRC

    uint32_t computed_crc = crc32_compute(data, data_len);

    return (computed_crc == *(uint32_t *)(data + data_len));}


