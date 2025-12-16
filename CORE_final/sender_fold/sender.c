// sender.c
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

// Function for reliable sending (No changes needed here)
int send_packet_reliably(int sock, struct sockaddr_in *dest_addr, DataPacket *pckt) {
    pckt->crc32 = crc32_compute(pckt->data, pckt->data_len);

    int attempts = 0;
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    AckPacket ack;

    while (1) {
        // 1. Send (Source port is determined by the socket bind in main)
        ssize_t sent = sendto(sock, pckt, sizeof(DataPacket), 0, (struct sockaddr*)dest_addr, sizeof(*dest_addr));
        if (sent < 0) {
            perror("Sendto failed");
            return 0;
        }

        // 2. Receive on the SAME socket
        int r = recvfrom(sock, &ack, sizeof(ack), 0, (struct sockaddr*)&from_addr, &from_len);

        if (r < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("Timeout (SEQ %u). Retransmitting... (Attempt %d)\n", pckt->seq, attempts + 1);
                attempts++;
                continue; 
            } else {
                perror("Socket error");
                return 0; 
            }
        }

        if (r == sizeof(AckPacket)) {
            // Check if it's the correct ACK
            if (ack.type == MSG_ACK && ack.seq == pckt->seq) {
                return 1; // Success
            }
        }
    }
}

int send_window_reliably(
    int sock,
    struct sockaddr_in *dest,
    DataPacket *packets,
    int packet_count,
    int window_size
) 
{
    int base = 0;
    int next = 0;

    typedef struct {
        int acked;
        struct timeval sent;
    } Slot;

    Slot slots[packet_count];
    memset(slots, 0, sizeof(slots));

    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    AckPacket ack;

    while (base < packet_count) {

        /* send new packets */
        while (next < base + window_size && next < packet_count) {
            sendto(sock, &packets[next], sizeof(DataPacket), 0,
                   (struct sockaddr*)dest, sizeof(*dest));
            gettimeofday(&slots[next].sent, NULL);
            next++;
        }

        /* wait for ACK */
        int r = recvfrom(sock, &ack, sizeof(ack), 0,
                         (struct sockaddr*)&from, &fromlen);

        if (r == sizeof(AckPacket) && ack.type == MSG_ACK) {
            int s = ack.seq;
            if (s >= 0 && s < packet_count)
                slots[s].acked = 1;

            while (base < packet_count && slots[base].acked)
                base++;
        }

        /* retransmit on timeout */
        struct timeval now;
        gettimeofday(&now, NULL);

        for (int i = base; i < next; i++) {
            if (slots[i].acked) continue;

            long ms =
                (now.tv_sec  - slots[i].sent.tv_sec ) * 1000 +
                (now.tv_usec - slots[i].sent.tv_usec) / 1000;

            if (ms > TIMEOUT_MS) {
                sendto(sock, &packets[i], sizeof(DataPacket), 0,
                       (struct sockaddr*)dest, sizeof(*dest));
                gettimeofday(&slots[i].sent, NULL);
            }
        }
    }
    return 1;
}


int main(int argc, char *argv[])
{
    // UPDATED ARGUMENT CHECK
    if (argc != 5) {
        printf("Usage: %s <Target_IP> <Target_Port> <Local_Port> <File>\n", argv[0]);
        printf("Example: %s 127.0.0.1 15000 60490 test.bin\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    int target_port = atoi(argv[2]); // Parse Target Port
    int local_port = atoi(argv[3]);  // Parse Local Port
    const char *filename = argv[4];

    if (target_port <= 0 || local_port <= 0) {
        printf("Error: Ports must be positive integers.\n");
        return 1;
    }

    // 1. Create Socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("Socket creation failed"); return 1; }

    // Allow re-binding immediately
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt failed");
    }

    // 2. BIND to the specific LOCAL PORT
    // This forces the Source Port of outgoing packets to be 'local_port'
    // And listens on 'local_port' for replies.
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(local_port); 
    local_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        perror("Bind failed (Port likely in use)");
        return 1;
    }

    printf("Socket bound. sending FROM port %d \n", local_port);

    // 3. Set Timeout
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = TIMEOUT_MS * 1000;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Setsockopt failed");
    }

    // 4. Define Target Address
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(target_port);
    inet_pton(AF_INET, ip, &dest_addr.sin_addr);

    // --- FILE HANDLING ---
    FILE *f = fopen(filename, "rb");
    if (!f) { perror("File not open"); return 1; }

    uint8_t file_hash[32];
    sha256_of_file(filename, file_hash); 

    fseek(f, 0, SEEK_END);
    long filesize = ftell(f);
    rewind(f);

    printf("Sending %s (%ld bytes) TO %s:%d\n", filename, filesize, ip, target_port);

    DataPacket pckt;
    uint32_t cntr = 0;

    // Send Metadata
    pckt.type = MSG_DATA;
    pckt.seq = cntr;
    snprintf((char*)pckt.data, DATA_MAX_SIZE, "FILENAME=%s;SIZE=%ld", filename, filesize);
    pckt.data_len = strlen((char*)pckt.data) + 1;

    send_packet_reliably(sock, &dest_addr, &pckt);
    cntr++;

    // Send File Data
    size_t bytes_sz;
    while ((bytes_sz = fread(pckt.data, 1, DATA_MAX_SIZE, f)) > 0) {
        pckt.type = MSG_DATA;
        pckt.seq = cntr;
        pckt.data_len = bytes_sz;

        send_packet_reliably(sock, &dest_addr, &pckt);
        
        printf("\rProgress: %ld/%ld bytes", ftell(f), filesize);
        fflush(stdout);
        cntr++;
    }
    printf("\n");

    // Send Hash
    pckt.type = MSG_HASH;
    pckt.seq = cntr;
    memcpy(pckt.data, file_hash, 32);
    pckt.data_len = 32;

    printf("Sending HASH...\n");
    send_packet_reliably(sock, &dest_addr, &pckt);

    printf("ALLDONE\n");
    fclose(f);
    close(sock);
    return 0;
}