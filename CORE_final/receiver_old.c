// receiver.c
#define RECEIVER

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>


#include "protocol.h"
#include "crc32.h"
#include "sha256.h"

void send_ack(int sock, struct sockaddr_in *target, uint32_t seq) {
    AckPacket ack;
    ack.type = MSG_ACK;
    ack.seq = seq;

    ack.crc = 0;

    sendto(sock, &ack, sizeof(ack), 0, (struct sockaddr*)target, sizeof(*target));
}

int main() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(LOCAL_PORT); // 5555 pro Receiver
    local_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    printf("Receiver running on port %d…\n", PORT);

    FILE *f = NULL;
    uint32_t expected_seq = 0;
    char output_filename[128] = "received_file.bin"; // Default
    uint8_t received_hash[32];
    int hash_received = 0;

    while (1) {
        DataPacket pckt;
        struct sockaddr_in sender_addr;


        int r = recvfrom(sock, &pckt, sizeof(pkt), 0, (struct sockaddr*)&sender_addr, &addr_len);
        if (r < 0) continue;
        }



        // CRC check
        uint32_t computed_crc = crc32_compute(pckt.data, pkt.data_len);
        if (computed_crc != pckt.crc32) {
            printf("CRC Mismatch on SEQ %u! Dropping packet.\n", pckt.seq);

            continue;
        }

        // packet number check
        if (pckt.seq > expected_seq) {

            continue;
        }


        if (pckt.type == MSG_DATA) {
            if (pckt.seq == 0 && strstr((char*)pckt.data, "FILENAME=") != NULL) {
                // outuput file get ready
                char *start = strstr((char*)pckt.data, "FILENAME=") + 9;
                char *end = strchr(start, ';');
                if (end) {
                    *end = '\0';
                    strcpy(output_filename, start);
                    f = fopen(output_filename, "wb");
                    printf("Receiving file: %s\n", output_filename);
                } else {
                    f = fopen(output_filename, "wb");
                }
            } else {
                // normal data
                if (f) fwrite(pckt.data, 1, pckt.data_len, f);
            }
            send_ack(sock, &sender_addr, pckt.seq);
            expected_seq++;

        } else if (pckt.type == MSG_HASH) {
            printf("Hash received \n");
            memcpy(received_hash, pckt.data, 32);
            hash_received = 1;
            send_ack(sock, &sender_addr, pckt.seq);
            break;

        }
    

    if (f) fclose(f);
    close(sock);

    if (hash_received) {
        printf("hash verification")
        uint8_t computed_file_hash[32];
        sha256_of_file(output_filename, computed_file_hash);
        if (memcmp(received_hash, computed_file_hash, 32) == 0) {
            printf("hash ok\n");
        } else {
            printf("ERROR:hash NOK\n");
        }
    }
    return 0;
}

