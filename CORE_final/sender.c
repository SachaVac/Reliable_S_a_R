// sender.c
#define SENDER


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>

#include "protocol.h"
#include "crc32.h"
#include "sha256.h"

#define TIMEOUT_MS 200

int send_packet(int socket, struct sockaddr_in *dest_addr, DataPacket *pckt) {
    pckt->crc32 = crc32_compute(pckt->data, pckt->data_len);

    int attempts = 0;
    struct sockaddr_in from_addr;

    socklen_t from_len = sizeof(from_addr);
    AckPacket ack;

    while (1) {
        sendto(socket, pckt, sizeof(DataPacket), 0, (struct sockaddr*)dest_addr, sizeof(*dest_addr));

        int back = recvfrom(sock, &ack, sizeof(ack), 0, (struct sockaddr*)&from_addr, &from_len);

        if (r < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("Timeout (SEQ %u). Retransmitting...\n", pkt->seq);
                attempts++;
                continue; 
            } else {
                perror("Socket error");
                return 0; // Kritická chyba
            }
        }

        if (r == sizeof(AckPacket)) {
            if (ack.type = MSG_ACK && ack.seq == pckt->seq) {
                return 1
            }
        }
    }


}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("Usage: %s <IP> <file>\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    const char *filename = argv[2];

    // UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(LOCAL_PORT);
    local_addr.sin_addr.sin_addr = INADDR_ANY;


    if (bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = TIMEOUT_MS * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(TARGET_PORT);
    inet_pton(AF_INET, ip, &dest_addr.sin_addr);

    FILE *f = fopen(filename, "rb");
    if (!f) { perror("File not open"); return 1; } // error messages

    // hash
    uint8_t file_hash[32];
    sha256_of_file(filename, file_hash);

    fseek(f, 0, SEEK_END);
    long filesize = ftell(f);
    rewind(f);

    printf("Sending %s (%ld bytes) to %s:%d\n", filename, filesize, ip, TARGET_PORT);


    DataPacket pckt;
    uint32_t cntr = 0;

    pckt.type = MSG_DATA;
    pckt.seq = cntr;
    snprintf((char*)pkt.data, DATA_MAX_SIZE, "FILENAME=%s;SIZE=%ld", filename, filesize);

    pckt.data_len = strlen((char*)pckt.data) + 1;

    send_packet(sock, &dest_addr, &pckt);

    cntr++;

    size_t bytes_sz;
    while ((bytes_sz = fread(pkt.data, 1, DATA_MAX_SIZE, f)) > 0) {
        pckt.type = MSG_DATA;
        pckt.seq = cntr;
        pckt.data_len = bytes_sz;

        send_packet(sock, &dest_addr, &pckt);
        printf("\rProgress: %ld/%ld bytes", ftell(f), filesize);
        fflush(stdout);
        cntr++;


    }
    printf("\n")

    pckt.type = MSG_HASH;
    pckt.seq = cntr;
    memcpy(pkt.data, file_hash, 32);
    pkt.data_len = 32;

    printf("HASH sent\n");
    send_packet_reliably(sock, &dest_addr, &pkt);

    printf("ALLDONE\n");
    fclose(f);
    close(sock);
    return 0;

}
