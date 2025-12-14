#ifndef SHA256_H
#define SHA256_H

#include <stdint.h>
#include <stddef.h>

void sha256_of_file(const char *filename, uint8_t out[32]);

#endif
