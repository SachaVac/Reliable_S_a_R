#include "sha256.h"
#include <openssl/sha.h>
#include <stdio.h>

void sha256_of_file(const char *filename, uint8_t out[32])
{
    FILE *f = fopen(filename, "rb");
    if (!f) return;

    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    uint8_t buf[4096];
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        SHA256_Update(&ctx, buf, n);

    SHA256_Final(out, &ctx);
    fclose(f);
}
