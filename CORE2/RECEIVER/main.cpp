#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#define HP_HASHVALUE 0x0002 // the default API key

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <wincrypt.h>
#include <direct.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")

#include "../include/messages.h"
#include "../include/crc32.h"

// =====================================================
// GLOBAL CRC-32 INSTANCE
// =====================================================

CRC32 g_crc32;

// =====================================================
// UTILITY FUNCTIONS
// =====================================================

std::string bytes_to_hex(const uint8_t* data, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    }
    return oss.str();
}

void print_header(const MessageHeader& header) {
    std::cout << "  type=" << header.type 
              << ", seq=" << header.sequenceNum 
              << ", crc=0x" << std::hex << header.checksum << std::dec << std::endl;
}

// =====================================================
// PACKET BUILDING AND VERIFICATION
// =====================================================

uint32_t compute_checksum(const uint8_t* buffer, size_t length) {
    return g_crc32.compute(buffer, length);
}

std::vector<uint8_t> build_packet(uint16_t msg_type, uint32_t seq, const uint8_t* payload = nullptr, size_t payload_len = 0) {
    std::vector<uint8_t> packet;
    
    // Create header with checksum = 0
    MessageHeader header{};
    header.type = msg_type;
    header._padding = 0;
    header.sequenceNum = seq;
    header.checksum = 0;
    
    packet.resize(sizeof(MessageHeader) + payload_len);
    std::memcpy(packet.data(), &header, sizeof(MessageHeader));
    
    if (payload && payload_len > 0) {
        std::memcpy(packet.data() + sizeof(MessageHeader), payload, payload_len);
    }
    
    // Compute CRC over packet with checksum field = 0
    uint32_t crc = compute_checksum(packet.data(), packet.size());
    
    // Update header with checksum
    header.checksum = crc;
    std::memcpy(packet.data(), &header, sizeof(MessageHeader));
    
    return packet;
}

// =====================================================
// UDP RECEIVER CLASS
// =====================================================

class Receiver {
private:
    SOCKET sock;
    std::string server_ip;
    uint16_t server_port;
    std::string local_ip;
    uint16_t local_port;
    int max_packet;
    
    static const int MAX_RETRIES = 15;
    static const int TIMEOUT_MS = 50;  // 0.05 seconds
    
public:
    Receiver(const std::string& ip, uint16_t port, uint16_t bind_port, int max_pkt)
        : server_ip(ip), server_port(port), local_port(bind_port), max_packet(max_pkt) {
        
        // Initialize Winsock
        WSADATA wsa_data;
        int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
        if (result != 0) {
            throw std::runtime_error("WSAStartup failed: " + std::to_string(result));
        }
        
        // Determine local IP
        local_ip = get_local_ip();
        
        // Create UDP socket
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET) {
            WSACleanup();
            throw std::runtime_error("socket() failed: " + std::to_string(WSAGetLastError()));
        }
        
        // Set socket timeout
        int timeout_val = TIMEOUT_MS;
        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_val, sizeof(timeout_val)) == SOCKET_ERROR) {
            closesocket(sock);
            WSACleanup();
            throw std::runtime_error("setsockopt() failed");
        }
        
        // Bind to local address (use INADDR_ANY for proper routing)
        sockaddr_in local_addr{};
        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.s_addr = INADDR_ANY;  // Allow system to choose source IP
        local_addr.sin_port = htons(local_port);

        if (bind(sock, (sockaddr*)&local_addr, sizeof(local_addr)) == SOCKET_ERROR) {
            closesocket(sock);
            WSACleanup();
            throw std::runtime_error("bind() failed: " + std::to_string(WSAGetLastError()));
        }        std::cout << "[INFO] Local IP: " << local_ip << std::endl;
        std::cout << "[OK] Bound to " << local_ip << ":" << local_port << std::endl;
        std::cout << "[OK] Talking to " << server_ip << ":" << server_port << std::endl;
    }
    
    ~Receiver() {
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
        }
        WSACleanup();
    }
    
    std::string get_local_ip() {
        // Simple approach: hardcoded or use hostname lookup
        // For now, return interface IP that can reach the server
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            hostent* host_entry = gethostbyname(hostname);
            if (host_entry) {
                in_addr addr{};
                addr.S_un.S_addr = *(uint32_t*)host_entry->h_addr_list[0];
                return inet_ntoa(addr);
            }
        }
        return "127.0.0.1";
    }
    
    void send_packet(const std::vector<uint8_t>& pkt) {
        std::cout << "[DEBUG] sending " << pkt.size() << " bytes from " 
                  << local_ip << ":" << local_port << " to " 
                  << server_ip << ":" << server_port << std::endl;
        
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);
        server_addr.sin_port = htons(server_port);
        
        int sent = sendto(sock, (const char*)pkt.data(), (int)pkt.size(), 0,
                         (sockaddr*)&server_addr, sizeof(server_addr));
        if (sent == SOCKET_ERROR) {
            throw std::runtime_error("sendto() failed: " + std::to_string(WSAGetLastError()));
        }
    }
    
    // ===== Protocol request methods =====
    
    void req_file_names() {
        auto pkt = build_packet(MSG_REQUEST_FILE_NAMES, 0);
        send_packet(pkt);
    }
    
    void req_name_seg(uint32_t seq) {
        uint32_t payload = seq;
        auto pkt = build_packet(MSG_REQUEST_TRANSFER_NAME, seq, (uint8_t*)&payload, sizeof(payload));
        send_packet(pkt);
    }
    
    void req_file(const std::string& name) {
        uint8_t payload[256]{};
        strcpy_s((char*)payload, sizeof(payload), name.c_str());
        auto pkt = build_packet(MSG_REQUEST_FILE, 0, payload, sizeof(payload));
        send_packet(pkt);
    }
    
    void req_file_seg(uint32_t seq) {
        uint32_t payload = seq;
        auto pkt = build_packet(MSG_REQUEST_TRANSFER_FILE, seq, (uint8_t*)&payload, sizeof(payload));
        send_packet(pkt);
    }
    
    void req_hash() {
        auto pkt = build_packet(MSG_REQUEST_HASH_FILE, 0);
        send_packet(pkt);
    }
    
    // ===== Receive and parse =====
    
    struct RecvResult {
        uint16_t msg_type;
        uint32_t seq;
        std::vector<uint8_t> payload;
        bool crc_ok;
        bool success;
    };
    
    RecvResult recv_packet(int expected_seq = -1) {
        RecvResult result{};
        result.success = false;
        result.crc_ok = true;

        uint8_t buffer[2048];
        sockaddr_in from{};
        int from_len = sizeof(from);

        int bytes = recvfrom(sock, (char*)buffer, sizeof(buffer), 0, (sockaddr*)&from, &from_len);

        if (bytes == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAETIMEDOUT) {
                std::cout << "[DEBUG] socket.timeout (timeout=" << (TIMEOUT_MS / 1000.0) << "s)" << std::endl;
            } else {
                std::cout << "[DEBUG] recvfrom failed: " << err << std::endl;
            }
            return result;
        }

        if (bytes < (int)sizeof(MessageHeader)) {
            std::cout << "[WARN] packet too small" << std::endl;
            return result;
        }

        // Parse header
        MessageHeader* header = (MessageHeader*)buffer;
        result.msg_type = header->type;
        result.seq = header->sequenceNum;

        std::cout << "Decoded header: type=" << header->type << ", seq=" << header->sequenceNum
                  << ", crc=0x" << std::hex << header->checksum << std::dec << std::endl;

        // Check expected sequence number
        if (expected_seq >= 0 && result.seq != (uint32_t)expected_seq) {
            std::cout << "[WARN] unexpected sequence number (expected " << expected_seq << ", got " << result.seq << ")" << std::endl;
            result.crc_ok = false;
        }

        // Verify CRC: reconstruct packet with checksum = 0
        uint32_t saved_crc = header->checksum;
        header->checksum = 0;
        uint32_t computed_crc = compute_checksum(buffer, bytes);

        if (computed_crc != saved_crc) {
            std::cout << "[WARN] CRC mismatch (expected 0x" << std::hex << saved_crc
                      << ", computed 0x" << computed_crc << std::dec << ")" << std::endl;
            result.crc_ok = false;
        }

        // Extract payload
        int payload_len = bytes - (int)sizeof(MessageHeader);
        if (payload_len > 0) {
            result.payload.assign(buffer + sizeof(MessageHeader), buffer + bytes);
        }

        result.success = true;
        return result;
    }    // ===== High-level file operations =====
    
    std::vector<std::string> get_file_names() {
        std::vector<std::string> names;
        std::cout << "[INFO] Requesting file names..." << std::endl;
        
        req_file_names();
        uint32_t next_seq = 1;

        while (true) {
            int retries = 0;
            while (retries <= MAX_RETRIES) {
                auto result = recv_packet(next_seq);                if (!result.success) {
                    retries++;
                    std::cout << "[WARN] timeout waiting for names (retry " << retries << "/" << MAX_RETRIES << ")" << std::endl;
                    req_name_seg(next_seq);
                    continue;
                }
                
                if (result.msg_type != MSG_ACK_FILE_NAMES) {
                    std::cout << "[WARN] unexpected message type " << result.msg_type << std::endl;
                    continue;
                }
                
                if (!result.crc_ok) {
                    retries++;
                    std::cout << "[WARN] bad CRC on name packet (retry " << retries << "/" << MAX_RETRIES << ")" << std::endl;
                    req_name_seg(next_seq);
                    continue;
                }
                
                // Parse name segment
                if (result.payload.size() < 258) {
                    std::cout << "[WARN] payload too small" << std::endl;
                    continue;
                }
                
                uint16_t remaining = *(uint16_t*)result.payload.data();
                std::string name((char*)(result.payload.data() + 2), 256);
                name = name.c_str();  // Null-terminate
                
                names.push_back(name);
                std::cout << "  -> " << name << std::endl;
                
                if (remaining == 1) {
                    return names;
                }
                
                next_seq++;
                req_name_seg(next_seq);
                break;
            }
            
            if (retries > MAX_RETRIES) {
                std::cout << "[ERROR] Failed to receive name segment after " << MAX_RETRIES << " retries" << std::endl;
                std::cout << "[INFO] Received " << names.size() << " file names before failure" << std::endl;
                return names;
            }
        }
    }
    
    std::vector<uint8_t> download_file(const std::string& name) {
        std::vector<uint8_t> data;
        std::cout << "[INFO] Requesting file '" << name << "'..." << std::endl;
        
        req_file(name);
        uint32_t seq_expected = 0;

        while (true) {
            int retries = 0;
            while (retries <= MAX_RETRIES) {
                auto result = recv_packet(seq_expected);                if (!result.success) {
                    retries++;
                    std::cout << "[WARN] timeout waiting for data (retry " << retries << "/" << MAX_RETRIES << ")" << std::endl;
                    if (seq_expected == 0) {
                        req_file(name);
                    } else {
                        req_file_seg(seq_expected);
                    }
                    continue;
                }
                
                if (result.msg_type != MSG_ACK_FILE_DATA) {
                    continue;
                }
                
                if (!result.crc_ok) {
                    retries++;
                    std::cout << "[WARN] bad CRC on data packet (retry " << retries << "/" << MAX_RETRIES << ")" << std::endl;
                    if (seq_expected == 0) {
                        req_file(name);
                    } else {
                        req_file_seg(seq_expected);
                    }
                    continue;
                }
                
                // Parse data segment
                if (result.payload.size() < 6) {
                    std::cout << "[WARN] payload too small for data segment" << std::endl;
                    continue;
                }
                
                uint32_t remaining = *(uint32_t*)(result.payload.data());
                uint16_t data_len = *(uint16_t*)(result.payload.data() + 4);
                
                if (6 + data_len <= result.payload.size()) {
                    data.insert(data.end(), 
                               result.payload.data() + 6,
                               result.payload.data() + 6 + data_len);
                }
                
                if (remaining == 1) {
                    return data;
                }
                
                seq_expected++;
                req_file_seg(seq_expected);
                break;
            }
            
            if (retries > MAX_RETRIES) {
                std::cout << "[ERROR] Failed to receive data after " << MAX_RETRIES << " retries" << std::endl;
                return data;
            }
        }
    }
    
    std::vector<uint8_t> recv_hash() {
        std::vector<uint8_t> hash;
        std::cout << "[INFO] Requesting MD5..." << std::endl;
        
        req_hash();
        int retries = 0;
        
        while (retries <= MAX_RETRIES) {
            auto result = recv_packet();
            
            if (!result.success) {
                retries++;
                std::cout << "[WARN] timeout waiting for hash (retry " << retries << "/" << MAX_RETRIES << ")" << std::endl;
                req_hash();
                continue;
            }
            
            if (result.msg_type != MSG_ACK_HASH_DATA) {
                continue;
            }
            
            if (!result.crc_ok) {
                retries++;
                std::cout << "[WARN] bad CRC on hash packet (retry " << retries << "/" << MAX_RETRIES << ")" << std::endl;
                req_hash();
                continue;
            }
            
            if (result.payload.size() >= 16) {
                hash.assign(result.payload.begin(), result.payload.begin() + 16);
            }
            
            return hash;
        }
        
        std::cout << "[ERROR] Failed to receive hash after retries" << std::endl;
        return hash;
    }
};

// =====================================================
// MAIN
// =====================================================

int main(int argc, char* argv[]) {
    try {
        // Get configuration from user or command line
        std::string server_ip = "192.168.8.149";
        uint16_t server_port = 7776;
        uint16_t local_port = 5556;
        int max_pkt = 1024;

        if (argc > 1) server_ip = argv[1];
        if (argc > 2) server_port = std::stoi(argv[2]);
        if (argc > 3) local_port = std::stoi(argv[3]);
        if (argc > 4) max_pkt = std::stoi(argv[4]);

        // If no command line args, prompt user (with defaults)
        if (argc == 1) {
            std::cout << "Server IP [" << server_ip << "]: ";
            std::string input;
            std::getline(std::cin, input);
            if (!input.empty()) server_ip = input;

            std::cout << "Server port [" << server_port << "]: ";
            std::getline(std::cin, input);
            if (!input.empty()) server_port = std::stoi(input);

            std::cout << "Local bind port [" << local_port << "]: ";
            std::getline(std::cin, input);
            if (!input.empty()) local_port = std::stoi(input);

            std::cout << "Max packet size [" << max_pkt << "]: ";
            std::getline(std::cin, input);
            if (!input.empty()) max_pkt = std::stoi(input);
        }

        // Create receiver
        Receiver receiver(server_ip, server_port, local_port, max_pkt);        // Get file list
        auto names = receiver.get_file_names();
        
        while (true) {
            // Display file list
            std::cout << "\nFiles available:" << std::endl;
            for (const auto& name : names) {
                std::cout << " - " << name << std::endl;
            }
            
            // Ask user which file to download
            std::cout << "\nWhich file to download? (or 'quit' to exit): ";
            std::string chosen;
            std::getline(std::cin, chosen);
            
            if (chosen == "quit" || chosen == "exit") {
                break;
            }
            
            // Check if file exists in list
            bool found = false;
            for (const auto& name : names) {
                if (name == chosen) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                std::cout << "Not found." << std::endl;
                continue;
            }
            
            // Download file
            auto file_data = receiver.download_file(chosen);
            std::cout << file_data.size() << " bytes downloaded." << std::endl;
            
            // Get hash from server
            auto server_hash = receiver.recv_hash();
            
            // Compute local hash
            HCRYPTPROV hProv = 0;
            HCRYPTHASH hHash = 0;
            DWORD hash_len = 16;
            BYTE local_hash[16];

            if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
                if (CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
                    CryptHashData(hHash, file_data.data(), (DWORD)file_data.size(), 0);
                    CryptGetHashParam(hHash, HP_HASHVALUE, local_hash, &hash_len, 0);
                    CryptDestroyHash(hHash);
                }
                CryptReleaseContext(hProv, 0);
            }            // Display hashes
            std::cout << "\nServer MD5: ";
            for (size_t i = 0; i < 16; i++) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)server_hash[i];
            }
            std::cout << std::dec << std::endl;
            
            std::cout << "Local  MD5: ";
            for (size_t i = 0; i < 16; i++) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)local_hash[i];
            }
            std::cout << std::dec << std::endl;
            
            bool match = std::equal(server_hash.begin(), server_hash.end(), local_hash);
            std::cout << "Match: " << (match ? "YES" : "NO") << std::endl;
            
            // Save file
            _mkdir("received");
            std::string out_name = std::string("received\\") + chosen;
            std::ofstream out(out_name, std::ios::binary);
            if (out) {
                out.write((const char*)file_data.data(), file_data.size());
                out.close();
                std::cout << "\nSaved as: " << out_name << std::endl;
            }
        }
        
        std::cout << "\nClosing connection..." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
