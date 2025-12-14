// receiver.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "protocol.h"
#include "crc32.h"
#include "sha256.h"

#define PORT 5000

int main()
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    bind(sock, (struct sockaddr*)&addr, sizeof(addr));

    printf("Receiver running on port %d…\n", PORT);

    char filename[128] = {0};
    long expected_size = 0;
    uint8_t expected_hash[32];

    FILE *f = NULL;
    uint32_t expected_seq = 0;

    while (1) {
        uint8_t buf[2048];
        int r = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &addrlen);

        if (r <= 0) continue;

        // zpracovávej meta-příkazy
        if (!memcmp(buf, "NAME=", 5)) {
            strcpy(filename, (char*)buf + 5);
            printf("File name: %s\n", filename);
            continue;
        }

        if (!memcmp(buf, "SIZE=", 5)) {
            expected_size = atol((char*)buf + 5);
            printf("File size: %ld\n", expected_size);
            continue;
        }

        if (!memcmp(buf, "HASH=", 5)) {
            printf("Received expected hash: %s\n", buf + 5);
            continue;
        }

        if (!memcmp(buf, "START", 5)) {
            printf("START received, opening file...\n");
            f = fopen("received.bin", "wb");
            continue;
        }

        if (!memcmp(buf, "STOP", 4)) {
            printf("STOP received.\n");
            break;
        }

        // DATA paket
        DataPacket *p = (DataPacket*)buf;

        uint32_t crc = crc32_compute(p->data, p->data_len);

        if (crc != p->crc32) {
            sendto(sock, "NACK", 4, 0, (struct sockaddr*)&addr, addrlen);
            continue;
        }

        if (p->seq == expected_seq) {
            fwrite(p->data, 1, p->data_len, f);
            expected_seq++;
        }

        sendto(sock, "ACK", 3, 0, (struct sockaddr*)&addr, addrlen);
    }

    if (f) fclose(f);
    close(sock);

    printf("File saved as received.bin\n");
    return 0;
}
