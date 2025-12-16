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

#define MAX_RECV_BUFFER 1024 // Maximum number of packets the receiver can buffer (Go-Back-N receiver typically doesn't need a large buffer, but this is defined for Selective Repeat potential)


uint16_t WINDOW_SIZE;

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

// receiver.c - New function to handle Go-Back-N receive logic


// Function to handle continuous receiving of packets using GBN logic
// It returns 1 on successful hash receipt, 0 otherwise.
// receiver.c - Nová funkce pro zpracování GBN přenosu
// Funkce pro zpracování příjmu paketů pomocí Selective Repeat (SR)
int receive_window_packets(
    int sock,
    int reply_port,
    FILE **f_ptr,
    char *output_filename,
    uint8_t *received_hash,
    int *hash_received_ptr
) 
{
    // Základ okna: Sekvenční číslo prvního očekávaného paketu
    uint32_t recv_base = 0; 
    
    // Velikost okna je WINDOW_SIZE, ale SR okno musí být menší nebo rovno polovině 
    // rozsahu sekvenčních čísel pro správnou funkčnost. Předpokládáme, že je WINDOW_SIZE definováno jinde.

    // Buffer pro ukládání paketů (uchovává aktuální okno)
    RecvSlot recv_buffer[WINDOW_SIZE];
    memset(recv_buffer, 0, sizeof(recv_buffer));

    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr); 
    DataPacket pckt;
    
    printf("\n--- Start SR Receive Loop (Expected BASE: 0) ---\n");

    while (1) {
        // 1. Příjem dat
        int r = recvfrom(sock, &pckt, sizeof(DataPacket), 0, (struct sockaddr*)&sender_addr, &addr_len);
        
        if (r < 0) {
            continue; 
        }

        // 2. Kontrola CRC
        uint32_t computed_crc = crc32_compute(pckt.data, pckt.data_len);
        if (computed_crc != pckt.crc32) {
            printf("CRC Mismatch on SEQ %u. Dropping packet.\n", pckt.seq);
            continue; // U SR příjemce neposílá nic, pokud je CRC špatné
        }
        
        // --- 3. SR Logika: Kontrola přijímacího okna ---
        uint32_t seq = pckt.seq;
        
        // Je paket v aktuálním přijímacím okně [recv_base, recv_base + WINDOW_SIZE - 1]?
        if (seq >= recv_base && seq < recv_base + WINDOW_SIZE) {
            
            // Paket je v okně: Uložíme ho do bufferu
            uint32_t buffer_index = seq % WINDOW_SIZE;
            
            if (recv_buffer[buffer_index].received == 0) {
                // Přišel nový platný paket, uložíme ho
                if (r == sizeof(DataPacket)) { // Zajišťuje uložení celé struktury
            memcpy(&recv_buffer[buffer_index].packet, &pckt, r);
            recv_buffer[buffer_index].received = 1;
            
            // ... (zbytek logiky ACK a printu)
            } else {
                printf("Received incomplete packet structure (%d bytes). Dropping.\n", r);
                continue; // Zahodit neúplný paket
            }
                
                
                // --- 4. Odeslání individuálního ACK ---
                // U SR posíláme ACK POUZE pro právě přijatý paket.
                send_ack(sock, &sender_addr, seq, reply_port); // ACK nese číslo PŘIJATÉHO paketu
                printf("\rSent ACK for SEQ %u.", seq);
                fflush(stdout);

            } else {
                // Duplicitní paket (již uložen a potvrzen).
                // Znovu odešleme ACK, pokud se ACK ztratilo (volitelné, ale užitečné)
                send_ack(sock, &sender_addr, seq, reply_port);
                printf("\nDuplicate SEQ %u received. Re-sending ACK.\n", seq);
            }

            // --- 5. Zpracování/Doručení dat a posun okna ---
            // Zpracováváme data A posouváme okno POUZE, pokud je přijat paket na pozici recv_base.
            while (recv_buffer[recv_base % WINDOW_SIZE].received == 1) {
                
                uint32_t current_index = recv_base % WINDOW_SIZE;
                DataPacket *current_pckt = &recv_buffer[current_index].packet;

                // 5.1 Zpracování obsahu
                if (current_pckt->type == MSG_DATA) {
                    // ... (logika pro metadata a zápis dat do souboru je stejná) ...
                    if (current_pckt->seq == 0) { // Metadata paket
                         char *start = strstr((char*)current_pckt->data, "FILENAME=") + 9;
                         char *end = strchr(start, ';');
                         if (end) {
                             *end = '\0'; 
                             strncpy(output_filename, start, 127);
                             output_filename[127] = '\0';
                             printf("Receiving file: %s\n", output_filename);
                             *f_ptr = fopen(output_filename, "wb");
                         }
                    } else {
                         // Zápis dat do souboru
                         if (*f_ptr) fwrite(current_pckt->data, 1, current_pckt->data_len, *f_ptr);
                    }
                }
                
                if (current_pckt->type == MSG_HASH) { // Hash Paket (Konec přenosu)
                // KONTROLA DÉLKY DAT
                    if (current_pckt->data_len != 32) {
                        printf("ERROR: Hash packet SEQ %u received with wrong length (%u). Ignoring.\n", 
                            current_pckt->seq, current_pckt->data_len);
                        // Neukončujeme, protože čekáme na správný hash. Posun okna se neprovede, dokud se to neopraví.
                    } else {
                        if (*f_ptr) fclose(*f_ptr);
                        *f_ptr = NULL;
                        
                        printf("HASH packet (SEQ %u) received. Closing file.\n", current_pckt->seq);
                        memcpy(received_hash, current_pckt->data, 32); 
                        *hash_received_ptr = 1;
                        
                        // Posun báze a vymazání slotu (pro pořádek)
                        recv_buffer[current_index].received = 0;
                        recv_base++; 
                        return 1; // Ukončení přenosu
                    }
                }
                
                // 5.2 Posun SR okna
                printf("Delivered SEQ %u. Sliding window.\n", recv_base);
                
                // Vyčistíme slot a posuneme bázi okna
                recv_buffer[current_index].received = 0;
                recv_base++;
            }
            
        } 
        // Je paket mimo okno? (Starý paket / Příliš daleký budoucí paket)
        else { 
            // C. Paket mimo okno - Znovu odešleme ACK pro starý paket, zahodíme budoucí.
            
            // Starý paket (již přijatý nebo se očekává vyšší)
            if (seq < recv_base) {
                // Pokud přišel paket, který už byl doručen (ACK se ztratil)
                send_ack(sock, &sender_addr, seq, reply_port);
                printf("\nOld SEQ %u received (Base %u). Re-sending ACK.\n", seq, recv_base);
            } else {
                // Příliš daleký budoucí paket (mimo aktuální okno)
                printf("\nOut-of-window SEQ %u received (Base %u). Dropping.\n", seq, recv_base);
            }
        }
    }
    return 0; // Kód by neměl být dosažen
}

int main(int argc, char *argv[]) {
    // 1. Argument Parsing
    if (argc != 4) {
        printf("Usage: %s <Local_Listen_Port> <Reply_To_Port> <WINDOW_SIZE>\n", argv[0]);
        printf("Example: %s 15000 60490 128\n", argv[0]);
        return 1;
    }

    int local_port = atoi(argv[1]);
    int reply_port = atoi(argv[2]);
    //int reply_addr = atoi(argv[3]);
    WINDOW_SIZE = (uint16_t)atoi(argv[3]); // Parse WINDOW_SIZE


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

    #ifdef STOP_AND_WAIT
    // STOP AND WAIT RECEIVING
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
    #else
    // GO-BACK-N RECEIVING
    if (receive_window_packets(
        sock,
        reply_port,
        &f, // Pass file pointer address
        output_filename,
        received_hash,
        &hash_received
    )) 
    {
        printf("\nFile transfer finished.\n");
    } else {
        printf("\nTransfer failed or ended prematurely.\n");
    }

    #endif

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