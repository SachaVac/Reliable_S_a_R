#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>
#include <stddef.h>

uint32_t crc32_compute(const uint8_t *data, size_t len);

// returns 1 if CRC matches, 0 otherwise
uint8_t crc32_verify(const uint8_t *data, size_t len);

#endif
