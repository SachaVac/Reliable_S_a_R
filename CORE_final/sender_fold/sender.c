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
    int base = 0;       // Sequence number of the first unACKed packet in the window
    int next_seq = 0;   // Sequence number to send next (upper boundary of the window)

    // A single timer for the packet at 'base' is sufficient for GBN.
    // However, since we are reusing the Slot structure, we will use 'sent' to track 
    // the time the current 'base' packet was last sent.
    typedef struct {
        int acked;      // Not strictly needed for GBN, but kept for consistency
        struct timeval sent; 
    } Slot;

    Slot slots[packet_count];
    memset(slots, 0, sizeof(slots));

    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    AckPacket ack;

    // --- Sliding Window Main Loop ---
    while (base < packet_count) {
        
        // 1. Send new packets (Fill the window)
        while (next_seq < base + window_size && next_seq < packet_count) {
            sendto(sock, &packets[next_seq], sizeof(DataPacket), 0,
                   (struct sockaddr*)dest, sizeof(*dest));
            
            // Start the timer for the packet at 'base' when it's first sent.
            if (base == next_seq) {
                 gettimeofday(&slots[base].sent, NULL);
                 // Optional: printf("Timer started for SEQ %d\n", base);
            }
            
            // Optional: printf("Sent packet: %d\n", packets[next_seq].seq);
            next_seq++;
        }

        // 2. Poll for ALL waiting ACKs
        // Loop continuously to drain the socket buffer until timeout is hit.
        while (1) {
            int r = recvfrom(sock, &ack, sizeof(ack), 0,
                             (struct sockaddr*)&from, &fromlen);

            if (r < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Timeout occurred, break out of the polling loop
                    break; 
                } else {
                    perror("Socket error during ACK reception");
                    return 0; // Fatal error
                }
            }
            
            // Received an ACK, process it (GBN Logic)
            if (r == sizeof(AckPacket) && ack.type == MSG_ACK) {
                // FIX: Assume ACK.seq is the next sequence number the receiver is waiting for.
                int new_base = ack.seq; 
                //printf("Received ACK for SEQ %d\n", new_base);
                // Only accept ACKs that advance the window (must be > base)
                if (new_base > base && new_base <= next_seq) {
                    
                    // Move the window base
                    base = new_base; // FIX: Base is directly set to the ACK value (e.g., 2985)
                    printf("ACK received. Window slid. New base: %d\n", base);

                    // Reset the timer for the *new* base packet (which is the first unACKed one)
                    if (base < packet_count) {
                        gettimeofday(&slots[base].sent, NULL);
                    }
                }
                // Příklad: Doplnění bloku pro řešení duplicitních ACK (kde new_base <= base)
                else if (new_base <= base && base < packet_count) {
                    // ACK potvrdil již potvrzené, ale Receiver nám říká, že něco chybí.
                    // Znovu odešleme 'base' paket, abychom se pokusili prolomit zaseknutí.
                    printf("Duplicate/Stale ACK for SEQ %d received. Resending base packet %d.\n", new_base, base);

                    sendto(sock, &packets[base], sizeof(DataPacket), 0,
                        (struct sockaddr*)dest, sizeof(*dest));
                        
                    gettimeofday(&slots[base].sent, NULL); // Restart časovače
            }

            }
            

        } // End of ACK polling loop

        // 3. Check for Timeout (Only the packet at 'base' needs a timer check in GBN)
        struct timeval now;
        gettimeofday(&now, NULL);

        if (base < packet_count) {
            long ms =
                (now.tv_sec - slots[base].sent.tv_sec ) * 1000 +
                (now.tv_usec - slots[base].sent.tv_usec) / 1000;

            if (ms > TIMEOUT_MS) {
                // GBN Retransmission: Retransmit ALL packets starting from 'base'
                printf("Timeout (SEQ %u). Retransmitting window from %d...\n", packets[base].seq, base);
                
                // Restart timer for the 'base' packet
                gettimeofday(&slots[base].sent, NULL);
                
                // Retransmit all unACKed packets in the current window
                for (int i = base; i < next_seq; i++) {
                    sendto(sock, &packets[i], sizeof(DataPacket), 0,
                           (struct sockaddr*)dest, sizeof(*dest));
                }
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

    
    // Send File Data
    size_t bytes_sz;

    
    // STOP AND WAIT SENDING
    DataPacket pckt;
    uint32_t cntr = 0;
    #ifdef STOP_AND_WAIT

    // Send Metadata
    pckt.type = MSG_DATA;
    pckt.seq = cntr;
    snprintf((char*)pckt.data, DATA_MAX_SIZE, "FILENAME=%s;SIZE=%ld", filename, filesize);
    pckt.data_len = strlen((char*)pckt.data) + 1;

    send_packet_reliably(sock, &dest_addr, &pckt);

    cntr++;


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
    
    #else
    // SLIDING WINDOW SENDING
    DataPacket *packets = NULL; // Pointer to the dynamically allocated array
    int pkt_cnt = 0;

    long num_data_packets_needed = (filesize + DATA_MAX_SIZE - 1) / DATA_MAX_SIZE;
    int total_packets_needed = (int)num_data_packets_needed + 1;

    packets = (DataPacket *)malloc(total_packets_needed * sizeof(DataPacket));
    if (packets == NULL) {
        perror("Failed to allocate memory for packets");
        fclose(f);
        close(sock);
        return 1;
    }

    // metadata 
    packets[pkt_cnt].type = MSG_DATA;
    packets[pkt_cnt].seq  = pkt_cnt;
    packets[pkt_cnt].data_len =
        snprintf((char*)packets[pkt_cnt].data, DATA_MAX_SIZE,
                "FILENAME=%s;SIZE=%ld", filename, filesize) + 1;
    pkt_cnt++;

    // file data 
    while ((bytes_sz = fread(packets[pkt_cnt].data, 1, DATA_MAX_SIZE, f)) > 0) {
        
        packets[pkt_cnt].type = MSG_DATA;
        packets[pkt_cnt].seq  = pkt_cnt;
        packets[pkt_cnt].data_len = bytes_sz;
        pkt_cnt++;
    }

    // Send Hash
    packets[pkt_cnt].type = MSG_HASH;
    packets[pkt_cnt].seq  = pkt_cnt;
    memcpy(packets[pkt_cnt].data, file_hash, 32);
    packets[pkt_cnt].data_len = 32;
    pkt_cnt++;


    // calculate CRCs for metadata
    for (int i = 0; i < pkt_cnt; i++)
        packets[i].crc32 = crc32_compute(packets[i].data,
                                     packets[i].data_len);

    send_window_reliably(
        sock,
        &dest_addr,
        packets,
        pkt_cnt,
        WINDOW_SIZE
    );

    free(packets);
    #endif

    printf("ALLDONE\n");
    fclose(f);
    close(sock);
    return 0;
}