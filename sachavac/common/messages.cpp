#include "messages.h"

uint16_t computeChecksum(const char* data, size_t length) {
    uint16_t sum = 0;
    for (size_t i = 0; i < length; i++) {
        sum += data[i];
    }
    return sum;
}

// --- Server / Sender handlers ---
void handleAckFileNames(uint32_t seqNum, uint16_t count) {
    logToFile("ACK File Names received, seq=" + std::to_string(seqNum) +
        ", count=" + std::to_string(count));
    // TODO: update internal state, mark event, etc.
}

void handleFileInfoAck(uint32_t seqNum, uint64_t fileSize, uint16_t segmentSize, uint32_t totalSegments) {
    logToFile("File Info ACK received, seq=" + std::to_string(seqNum) +
        ", fileSize=" + std::to_string(fileSize) +
        ", segmentSize=" + std::to_string(segmentSize) +
        ", totalSegments=" + std::to_string(totalSegments));
    // TODO: update internal state, trigger EV_ACK_FILE_INFO_SENT
}

void handleAckData(uint32_t seqNum) {
    logToFile("ACK Data received, seq=" + std::to_string(seqNum));
    // TODO: mark segment as acknowledged
}

// --- Client / Receiver handlers ---
void handleRequestFileNames(uint32_t seqNum) {
    logToFile("Request File Names received, seq=" + std::to_string(seqNum));
    // TODO: prepare FileNamesAck reply
}

void handleFileRequest(uint32_t seqNum, const char* fileName) {
    logToFile("File Request received, seq=" + std::to_string(seqNum) +
        ", fileName=" + std::string(fileName));
    // TODO: prepare FileInfoAck reply
}

void handleStartTransfer(uint32_t seqNum) {
    logToFile("Start Transfer received, seq=" + std::to_string(seqNum));
    // TODO: trigger EV_TRANSFER_STARTED
}

void handleDataSegment(uint32_t seqNum, const uint8_t* data, uint16_t dataSize) {
    logToFile("Data Segment received, seq=" + std::to_string(seqNum) +
        ", size=" + std::to_string(dataSize));
    // TODO: copy data, process segment, send ACK_DATA
}

void handleCloseConnection(uint32_t seqNum) {
    logToFile("Close Connection received, seq=" + std::to_string(seqNum));
    // TODO: cleanup state, send final ACK if needed
}




void handleReceivedPacket(const char* buffer, size_t length, EventMessage * ev)
    {
        if (length < sizeof(MessageHeader) || !ev) return; // sanity check

        const MessageHeader* header = reinterpret_cast<const MessageHeader*>(buffer);

        // Verify checksum of the whole message (header + payload)
        uint16_t received = header->checksum;
        uint16_t calculated = computeChecksum(buffer, length); // use full length
        if (received != calculated) {
            logToFile("Received corrupted packet, checksum mismatch");
            return;
        }

        // Fill the EventMessage header
        //ev->type = static_cast<EventType>(header->type);

        // Fill payload pointer based on message type
        switch (header->type) {

        case MSG_ACK_FILE_NAMES: {
            // Payload points to FileNamesAck followed by optional DataSegment.data
            ev->payload = reinterpret_cast<const FileNamesAck*>(buffer + sizeof(MessageHeader));
            const FileNamesAck* msg = reinterpret_cast<const FileNamesAck*>(ev->payload);
            handleAckFileNames(header->sequenceNum, msg->count);
            break;
        }

        case MSG_ACK_FILE_INFO: {
            ev->payload = reinterpret_cast<const FileInfoAck*>(buffer + sizeof(MessageHeader));
            const FileInfoAck* msg = reinterpret_cast<const FileInfoAck*>(ev->payload);
            handleFileInfoAck(header->sequenceNum, msg->fileSize, msg->segmentSize, msg->totalSegments);
            break;
        }

        case MSG_REQUEST_FILE_NAMES: {
            ev->payload = nullptr;
            handleRequestFileNames(header->sequenceNum);
            break;
        }

        case MSG_REQUEST_FILE: {
            ev->payload = reinterpret_cast<const DataSegment*>(buffer + sizeof(MessageHeader));
            const DataSegment* msg = reinterpret_cast<const DataSegment*>(ev->payload);
            // fileName is assumed to be in msg->data as null-terminated string
            handleFileRequest(header->sequenceNum, reinterpret_cast<const char*>(msg->data));
            break;
        }

        case MSG_START_TRANSFER: {
            ev->payload = nullptr;  // no payload
            handleStartTransfer(header->sequenceNum);
            break;
        }

        case MSG_DATA: {
            ev->payload = reinterpret_cast<const DataSegment*>(buffer + sizeof(MessageHeader));
            const DataSegment* msg = reinterpret_cast<const DataSegment*>(ev->payload);
            handleDataSegment(msg->header.sequenceNum, msg->data, msg->payloadLength);
            break;
        }

        case MSG_ACK_DATA: {
            ev->payload = nullptr; // no additional payload
            handleAckData(header->sequenceNum);
            break;
        }

        case MSG_CLOSE_CONNECTION: {
            ev->payload = nullptr;
            handleCloseConnection(header->sequenceNum);
            break;
        }

        default: {
            ev->payload = nullptr;
            break;
        }
}
