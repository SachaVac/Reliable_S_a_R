#include "sha256.h"

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

void sha256_of_file_(FILE *f, uint8_t out[32])
{
    if (!f) return;

    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    uint8_t buf[4096];
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        SHA256_Update(&ctx, buf, n);

    SHA256_Final(out, &ctx);
}

uint8_t sha256_verify(const uint8_t *data, size_t len, uint8_t compare[32])
{
    uint8_t sha[32];
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, data, len);
    SHA256_Final(sha, &ctx);

    return (memcmp(sha, compare, 32) == 0);
}
