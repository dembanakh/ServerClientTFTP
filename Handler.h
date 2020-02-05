#pragma once

#include "config.h"
#include "ClientHandler.h"
#include "IHandler.h"
#include "IOManager.h"

class ClientHandler;
class IHandler;
class IOManager;

class Handler : public IHandler {
	map<int, ClientHandler*> clients;
protected:
	void acceptNewClient(int, char*, ssize_t, sockaddr_in*, socklen_t*);
public:
	Handler(IOManager * io) : IHandler(io) {}
	void handle(int, bool);
	void removeClient(int);
	void handleTimeout();
};
