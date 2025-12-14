// Assumed necessary headers for this file
#include "sender.h"

// Assuming these definitions are available via inclusion of UdpStructs.h and UdpMessages.h


// Assuming the structs are defined as follows (for context):
// struct MessageHeader { uint16_t type; uint32_t sequenceNum; };
// struct FileInfo { MessageHeader header; uint64_t fileSize; uint16_t segmentSize; uint32_t totalSegments; };


// --- Start of Implementation for sendFileInfo ---

/**
 * @brief Prepares and sends a FileInfo (MSG_ACK_FILE_INFO) packet back to the client.
 * * * It loads the size of "default.stl", calculates segment count, and sends the metadata.
 * * NOTE: The function signature is adjusted to accept 'sockaddr_in*' for network ops.
 * * @param from Pointer to the sockaddr_in structure containing the client's address.
 * This structure's port will be modified to match the replyPort.
 * @param fileName The name of the file requested by the client (will be "default.stl").
 * @param replyPort The port number on the client side where the reply should be sent.
 */
//void sendFileInfo(struct sockaddr_in* from,
//    const char* fileName,
//    int replyPort)
//{
//    FileInfo fileInfoPacket;
//    std::string clientIP = inet_ntoa(from->sin_addr);
//    const std::string defaultFileName = "default.stl";
//
//    // ----------------------------------------------------------------------
//    // 1. FILE SYSTEM LOGIC: Ensure file exists and get its size
//    // ----------------------------------------------------------------------
//    uint64_t actualFileSize = 0;
//    bool fileExists = false;
//
//    // Check the original logic's intent (which seems to be: if not 'default' 
//    // export it, otherwise proceed. We assume the intention is to use default.stl)
//    if (std::string(fileName) != "default")
//    {
//        // This suggests an export is needed if a specific design is requested
//        exportDesignAsSTL(defaultFileName.c_str());
//    }
//    // Else branch is empty, which implies 'default.stl' is expected to exist
//    // if a non-default file wasn't requested. We will proceed to load it regardless.
//
//    // Load file size for "default.stl"
//    std::ifstream file(defaultFileName, std::ios::binary | std::ios::ate);
//
//    if (file.is_open()) {
//        actualFileSize = file.tellg();
//        file.close();
//        fileExists = true;
//    }
//
//    if (!fileExists || actualFileSize == 0) {
//        logToFile("ERROR: Target file '" + defaultFileName + "' not found or is empty.");
//        // Implement sending a negative acknowledgment packet (e.g., MSG_FILE_NOT_FOUND) here.
//        return;
//    }
//
//    // Calculate total segments needed
//    // Using ceil(a/b) is equivalent to (a + b - 1) / b for integer arithmetic
//    uint32_t totalSegments = (uint32_t)ceil((double)actualFileSize / MAX_DATA_PAYLOAD_SIZE);
//
//    // ----------------------------------------------------------------------
//    // 2. CONSTRUCT THE PACKET
//    // ----------------------------------------------------------------------
//    fileInfoPacket.header.type = MSG_ACK_FILE_INFO;
//    fileInfoPacket.header.sequenceNum = 0; // Handshake packets typically use Seq=0
//    fileInfoPacket.fileSize = actualFileSize;
//    fileInfoPacket.segmentSize = MAX_DATA_PAYLOAD_SIZE;
//    fileInfoPacket.totalSegments = totalSegments;
//
//    // Log constructed metadata
//    logToFile("Sending FileInfo for " + defaultFileName + " to " + clientIP +
//        ": Size=" + std::to_string(actualFileSize) +
//        ", Segments=" + std::to_string(totalSegments) +
//        ", SegSize=" + std::to_string(MAX_DATA_PAYLOAD_SIZE));
//
//    // ----------------------------------------------------------------------
//    // 3. SET DESTINATION AND SEND
//    // ----------------------------------------------------------------------
//
//    // Set the destination port to the client's expected listening port (replyPort)
//    from->sin_port = htons(replyPort);
//    int fromLen = sizeof(struct sockaddr_in);
//
//    // Send the structured packet
//    int bytesSent = sendto(socketS,
//        (const char*)&fileInfoPacket,
//        sizeof(FileInfo),
//        0,
//        (sockaddr*)from,
//        fromLen);
//
//    if (bytesSent == SOCKET_ERROR) {
//        logToFile("FAILED to send FileInfo to " + clientIP + ". Error: " + std::to_string(WSAGetLastError()));
//    }
//    else {
//        logToFile("Successfully sent " + std::to_string(bytesSent) + " bytes (FileInfo) to client.");
//    }
//}

// Send a message over UDP and log the result
int sendMsg(Message* msg, const sockaddr_in* to, int port, SOCKET socketS)
{
    if (!msg || !to) return SOCKET_ERROR;

    // Convert payload to raw bytes
    const char* data = reinterpret_cast<const char*>(msg);
    int length = sizeof(MessageHeader); // send only header if no payload
    if (msg->payload)
    {
        // Add payload size if known; for example, use msg->header.length if you have it
        // Here we assume a simple fixed size or known protocol
        length += 1024; // adjust according to actual payload size
    }

    std::string clientIP = inet_ntoa(to->sin_addr);

    // Logging: Attempt to send
    logToFile("Attempting to send reply to " + clientIP);

    int sent = sendto(socketS, data, length, 0,
        (sockaddr*)to, sizeof(*to));

    if (sent == SOCKET_ERROR)
    {
        logToFile("FAILED to send reply to " + clientIP +
            ". Error code: " + std::to_string(WSAGetLastError()));
    }
    else
    {
        logToFile("Successfully sent " + std::to_string(sent) +
            " bytes to " + clientIP);
    }

    return sent;
}


uint8_t sendAndWaitResponse(Message* msg, EventMessage* ev, const char* from, int replyPort)
{
    for (int attempt = 0; attempt < MAX_RETRIES; ++attempt)
    {
        // 1. Send the message
        //sendMsg(msg, from, replyPort); 

        // 2. Wait for a response (simplified)
        std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT_MS));

        // 3. Check if a packet was received
        int bytesReceived = receivePacket(buffer, sizeof(buffer), from, replyPort);
        if (bytesReceived > 0)
        {
            handleReceivedPacket(buffer, bytesReceived, ev); // parses packet into ev
            handleEvent(*ev, msg);                           // process the event
            return true;                                     // success
        }

        // Optionally log retry attempt
        logToFile("Retry " + std::to_string(attempt + 1));
    }

    // Failed after MAX_RETRIES
    logToFile("No response received after " + std::to_string(MAX_RETRIES) + " attempts");
    return 0;
}