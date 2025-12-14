// receiver.c
#define RECEIVER

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "protocol.h"
#include "crc32.h"  // Musíte mít tento soubor
#include "sha256.h" // Musíte mít tento soubor

void send_ack(int sock, struct sockaddr_in *target, uint32_t seq) {
    AckPacket ack;
    ack.type = MSG_ACK;
    ack.seq = seq;
    ack.crc32 = 0; // Opraveno jméno položky struktury

    sendto(sock, &ack, sizeof(ack), 0, (struct sockaddr*)target, sizeof(*target));
}

int main() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("Socket failed"); return 1; }

    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(LOCAL_PORT); 
    local_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    printf("Receiver running on port %d...\n", LOCAL_PORT);

    FILE *f = NULL;
    uint32_t expected_seq = 0;
    char output_filename[128] = "receiived_file.jpg"; 
    uint8_t received_hash[32];
    int hash_received = 0;

    while (1) {
        DataPacket pckt;
        struct sockaddr_in sender_addr;
        socklen_t addr_len = sizeof(sender_addr); // Nutná inicializace!

        int r = recvfrom(sock, &pckt, sizeof(pckt), 0, (struct sockaddr*)&sender_addr, &addr_len);
        if (r < 0) continue;

        // CRC check
        uint32_t computed_crc = crc32_compute(pckt.data, pckt.data_len);
        if (computed_crc != pckt.crc32) {
            printf("CRC Mismatch on SEQ %u! Dropping packet.\n", pckt.seq);
            continue;
        }

        // Duplikace (už máme)
        if (pckt.seq < expected_seq) {
             send_ack(sock, &sender_addr, pckt.seq);
             continue;
        }

        // Paket z budoucnosti (zahodit)
        if (pckt.seq > expected_seq) {
            continue;
        }

        // Správný paket (pckt.seq == expected_seq)
        if (pckt.type == MSG_DATA) {
            if (pckt.seq == 0 && strstr((char*)pckt.data, "FILENAME=") != NULL) {
                
                // Parse filename
                char *start = strstr((char*)pckt.data, "FILENAME=") + 9;
                char *end = strchr(start, ';');
                
                if (end) {
                    *end = '\0'; // Ukončení stringu na konci jména (za ';')

                    // --- ZDE JE ŘEŠENÍ: KOPÍROVÁNÍ PŘIJATÉHO NÁZVU VČETNĚ PŘÍPONY ---
                    // Kopírování názvu, který odesílatel poslal (např. 'moje_foto.jpg')
                    strncpy(output_filename, start, sizeof(output_filename) - 1);
                    output_filename[sizeof(output_filename) - 1] = '\0'; // Zajištění ukončení stringu
                    
                    f = fopen(output_filename, "wb");
                    printf("Receiving file: %s\n", output_filename);
                } else {
                    // Selhání formátu, použití výchozího názvu
                    f = fopen(output_filename, "wb");
                    printf("Receiving file: %s (Using default due to format error)\n", output_filename);
                }
            } else {
                // Write data
                if (f) fwrite(pckt.data, 1, pckt.data_len, f);
            }
            send_ack(sock, &sender_addr, pckt.seq);
            expected_seq++;

        } else if (pckt.type == MSG_HASH) {
            printf("Hash received \n");
            memcpy(received_hash, pckt.data, 32);
            hash_received = 1;
            send_ack(sock, &sender_addr, pckt.seq);
            break; // Konec přenosu
        }
    }

    if (f) fclose(f);
    close(sock);

    if (hash_received) {
        printf("Hash verification...\n");
        uint8_t computed_file_hash[32];
        // Pozor: Zde se předpokládá, že soubor je již uložen na disku
        sha256_of_file(output_filename, computed_file_hash);
        
        if (memcmp(received_hash, computed_file_hash, 32) == 0) {
            printf("HASH OK\n");
        } else {
            printf("ERROR: HASH NOK\n");
        }
    }
    return 0;
}