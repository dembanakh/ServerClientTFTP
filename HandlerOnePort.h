#pragma once

#include "config.h"
#include "IHandler.h"
#include "IOManager.h"
#include "ClientHandlerOnePort.h"

class IHandler;
class IOManager;
class ClientHandlerOnePort;

class HandlerOnePort : public IHandler {
	map<string, ClientHandlerOnePort*> clients;
protected:
	void acceptNewClient(int, char*, ssize_t, sockaddr_in*, socklen_t*);
public:
	HandlerOnePort(IOManager * io) : IHandler(io) {}
	void handle(int, bool);
	void removeClient(sockaddr*);
	void handleTimeout();
};


