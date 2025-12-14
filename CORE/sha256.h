#ifndef SHA256_H
#define SHA256_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <openssl/sha.h>

void sha256_of_file(const char *filename, uint8_t out[32]);

void sha256_of_file_(FILE *f, uint8_t out[32]);
// returns 1 if hash matches, 0 otherwise
void sha256_verify(const uint8_t *data, size_t len, uint8_t compare[32]);

#endif
