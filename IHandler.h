#pragma once

#include "IOManager.h"
#include "config.h"

class IOManager;

class IHandler {
protected:
	IOManager * io;
	int fd;
	sockaddr_in addr;
	socklen_t addr_size;

	int buffer_size = 512;

	void init();
	bool receiveRequest(char*, int, request*);
	virtual void acceptNewClient(int a, char* b, int c, sockaddr_in * d, socklen_t * e) = 0;
public:
	IHandler(IOManager * io) {
		this->io = io;
		init();
	}
	~IHandler() { close(fd); };
	virtual void handle(int a, bool b) = 0;
	virtual void handleTimeout() = 0;
};
