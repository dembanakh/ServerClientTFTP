#pragma once

#include "config.h"
#include "IClientHandler.h"
#include "IOManager.h"
#include "Handler.h"

class IClientHandler;
class IOManager;
class Handler;

class ClientHandler : public IClientHandler {
protected:
	void removeSelf();
public:
	ClientHandler(request & _r, int _fd, sockaddr _addr, IHandler * h) {
		r = _r;
		fd = _fd;
		addr = _addr;
		addr_size = sizeof(addr);
		block_number = (r.type == WRQ) ? 0 : 1;
		handler = h;

		char mode[2] {};
		mode[0] = (r.type == RRQ) ? 'r' : 'w';
		mode[1] = (r.type == NETASCII) ? 't' : 'b';
		f = fopen(r.filename.c_str(), mode);
		if (f == NULL) error = true;

		block_number = 1;
		last_block = 0;
	}

	void start() {
		if (error) {
			cout << "server: There is no file requested - aborting communication" << endl;
			removeSelf();
			return;
		}
		IOManager::addEvent(fd, 0); // EPOLLET
		if (sendOACK()) {
			block_number = 1;
			if (r.type == RRQ) last_block = -1;
			if (error) {
				fclose(f);
				removeSelf();
			}
			return;
		} else oack_received = true;
		if (r.type == WRQ) {
			block_number = 0;
			sendACK();
		} else {
			sendData();
		}
		if (error) {
			cout << "server: Fatal error - aborting communication" << endl;
			fclose(f);
			removeSelf();
		}
	}
};
