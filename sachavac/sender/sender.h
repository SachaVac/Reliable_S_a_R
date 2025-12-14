#pragma once
#include <winsock2.h>
#include <WS2tcpip.h>
#include <string>
#include <cmath>
#include <fstream>      // For file I/O
#include <algorithm>    // For std::min/max

#include "messages.h"
#include "events.h"
#include "logger.h"


extern SOCKET socketS;
//extern void exportDesignAsSTL(const char* fileName); // Assuming this exists

//void sendFileInfo(struct sockaddr_in* from, const char* fileName, int replyPort);


uint8_t sendAndWaitResponse(Message* msg, EventMessage* ev, const char* from, int replyPort);

int sendMsg(Message* msg, const sockaddr_in* to, int port, SOCKET socketS);

