// receiver.c
#define RECEIVER

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

#include "protocol.h"
#include "crc32.h"  
#include "sha256.h" 

// Modified send_ack to accept the specific reply_port
void send_ack(int sock, struct sockaddr_in *target, uint32_t seq, int reply_port) {
    AckPacket ack;
    ack.type = MSG_ACK;
    ack.seq = seq;
    ack.crc32 = 0; 

    // FORCE the destination port for the ACK
    // This overwrites the port derived from the incoming packet
    #ifdef LOCAL
    target->sin_port = htons(reply_port); 
    #endif

    sendto(sock, &ack, sizeof(ack), 0, (struct sockaddr*)target, sizeof(*target));
}

int main(int argc, char *argv[]) {
    // 1. Argument Parsing
    if (argc != 3) {
        printf("Usage: %s <Local_Listen_Port> <Reply_To_Port>\n", argv[0]);
        printf("Example: %s 15000 60490\n", argv[0]);
        return 1;
    }

    int local_port = atoi(argv[1]);
    int reply_port = atoi(argv[2]);
    //int reply_addr = atoi(argv[3]);

    if (local_port <= 0 || reply_port <= 0) {
        printf("Error: Ports must be positive integers.\n");
        return 1;
    }

    // 2. Create Socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("Socket failed"); return 1; }

    // Allow re-binding
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt failed");
    }

    // 3. Bind to Local Port
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(local_port); 
    local_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    printf("Receiver listening on port %d. Will reply to port %d.\n", local_port, reply_port);

    FILE *f = NULL;
    uint32_t expected_seq = 0;
    char output_filename[128] = "received_file.bin"; // Default name
    uint8_t received_hash[32];
    int hash_received = 0;

    while (1) {
        DataPacket pckt;
        struct sockaddr_in sender_addr;
        socklen_t addr_len = sizeof(sender_addr); 

        // Receive data
        int r = recvfrom(sock, &pckt, sizeof(pckt), 0, (struct sockaddr*)&sender_addr, &addr_len);
        if (r < 0) {
            // Optional: Print error if it's not just a timeout
            perror("recvfrom error");
            continue;
        }

        // CRC check
        uint32_t computed_crc = crc32_compute(pckt.data, pckt.data_len);
        if (computed_crc != pckt.crc32) {
            printf("CRC Mismatch on SEQ %u! Dropping packet.\n", pckt.seq);
            continue;
        }

        // Duplicate handling (already received this packet)
        if (pckt.seq < expected_seq) {
             send_ack(sock, &sender_addr, pckt.seq, reply_port);
             continue;
        }

        // Future packet (out of order - simple Stop-and-Wait logic drops this)
        if (pckt.seq > expected_seq) {
            continue;
        }

        // Correct packet (pckt.seq == expected_seq)
        if (pckt.type == MSG_DATA) {
            // Check for Metadata (First packet usually contains filename)
            if (pckt.seq == 0 && strstr((char*)pckt.data, "FILENAME=") != NULL) {
                
                char *start = strstr((char*)pckt.data, "FILENAME=") + 9;
                char *end = strchr(start, ';');
                
                if (end) {
                    *end = '\0'; 
                    strncpy(output_filename, start, sizeof(output_filename) - 1);
                    output_filename[sizeof(output_filename) - 1] = '\0'; 
                    
                    f = fopen(output_filename, "wb");
                    printf("Receiving file: %s\n", output_filename);
                } else {
                    f = fopen(output_filename, "wb");
                    printf("Receiving file: %s (Default due to parsing error)\n", output_filename);
                }
            } else {
                // Write data to file
                if (f) fwrite(pckt.data, 1, pckt.data_len, f);
            }
            
            // Send ACK using the configured reply_port
            send_ack(sock, &sender_addr, pckt.seq, reply_port);
            expected_seq++;

        } else if (pckt.type == MSG_HASH) {
            printf("Hash received.\n");
            memcpy(received_hash, pckt.data, 32);
            hash_received = 1;
            send_ack(sock, &sender_addr, pckt.seq, reply_port);
            break; // End of transfer
        }
    }

    if (f) fclose(f);
    close(sock);

    // Verify Hash
    if (hash_received) {
        printf("Verifying Hash...\n");
        uint8_t computed_file_hash[32];
        
        // Ensure sha256_of_file handles the file path correctly
        sha256_of_file(output_filename, computed_file_hash);
        if (memcmp(received_hash, computed_file_hash, 32) == 0) {
            printf("SUCCESS: File integrity verified (HASH OK).\n");
        } else {
            printf("ERROR: Hash mismatch! File may be corrupted.\n");
        }
    
    }
    return 0;
}