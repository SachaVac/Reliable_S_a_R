// sender.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include "protocol.h"
#include "crc32.h"
#include "sha256.h"

#define PORT 5000

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("Usage: %s <IP> <file>\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    const char *filename = argv[2];

    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("File open error");
        return 1;
    }

    // UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    printf("Sending file: %s\n", filename);

    uint8_t sha_out[32];
    sha256_of_file(filename, sha_out);

    // Pošli metadata
    char meta[256];
    snprintf(meta, sizeof(meta), "NAME=%s", filename);
    sendto(sock, meta, strlen(meta), 0, (struct sockaddr*)&addr, addrlen);

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    rewind(f);

    snprintf(meta, sizeof(meta), "SIZE=%ld", file_size);
    sendto(sock, meta, strlen(meta), 0, (struct sockaddr*)&addr, addrlen);

    char hash_hex[100];
    for (int i = 0; i < 32; i++) sprintf(hash_hex + 2*i, "%02x", sha_out[i]);

    snprintf(meta, sizeof(meta), "HASH=%s", hash_hex);
    sendto(sock, meta, strlen(meta), 0, (struct sockaddr*)&addr, addrlen);

    sendto(sock, "START", 5, 0, (struct sockaddr*)&addr, addrlen);

    // Stop-and-Wait
    DataPacket packet;
    uint32_t seq = 0;

    while (!feof(f)) {

        packet.seq = seq;
        packet.data_len = fread(packet.data, 1, DATA_MAX_SIZE, f);
        packet.crc32 = crc32_compute(packet.data, packet.data_len);

        // odesílej dokud nepřijde ACK
        while (1) {
            sendto(sock, &packet, sizeof(packet), 0, (struct sockaddr*)&addr, addrlen);

            char ack[32] = {0};
            int r = recvfrom(sock, ack, sizeof(ack), MSG_DONTWAIT, NULL, NULL);

            if (r > 0 && strstr(ack, "ACK") != NULL) {
                printf("ACK %u\n", seq);
                break;
            }

            usleep(10000); // 10ms retry
        }

        seq++;
    }

    sendto(sock, "STOP", 4, 0, (struct sockaddr*)&addr, addrlen);

    fclose(f);
    close(sock);
    return 0;
}
