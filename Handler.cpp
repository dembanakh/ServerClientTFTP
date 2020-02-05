#include "Handler.h"
#include "config.h"
#include "ClientHandler.h"

class ClientHandler;

void Handler::handle(int _fd, bool in) {
	if (_fd == fd && in) {
		sockaddr_in income;
		socklen_t income_size;
		char buf[buffer_size] {};
		ssize_t s = recvfrom(fd, buf, buffer_size, 0, (sockaddr*)&income, &income_size);
		
		if (IS_RRQ(buf)) acceptNewClient(RRQ, buf, s, &income, &income_size);
		else if (IS_WRQ(buf)) acceptNewClient(WRQ, buf, s, &income, &income_size);
		else return; //error
	} else {
		auto cl = clients.find(_fd);
		if (cl != clients.end()) {
			ClientHandler * client = cl->second;
			if (in) {
				sockaddr_in income;
				socklen_t income_size;
				char buf[buffer_size] {};
				ssize_t s = recvfrom(_fd, buf, buffer_size, 0, (sockaddr*)&income, &income_size);
					
				if (IS_DATA(buf)) client->notify(DATA, buf, s);
				else if (IS_ACK(buf)) client->notify(ACK, buf, s);
				else return; //rubbish in fd
			}
		}
	}
}

void Handler::acceptNewClient(int req_type, char * buf, ssize_t buf_size, sockaddr_in * req_addr, socklen_t * req_size) {
	request r;
	r.type = req_type;
	cout << "Received " << (req_type == RRQ ? "Read" : "Write") << " request" << endl;
	if (!receiveRequest(buf, buf_size, &r)) return;
	int client_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (client_fd < 0) {
		perror("Failed to create client fd");
		return;
	}
	sockaddr_in client_addr;
	client_addr.sin_family = AF_INET;
	client_addr.sin_addr = req_addr->sin_addr;
	client_addr.sin_port = req_addr->sin_port;
	/*if (bind(client_fd, (sockaddr*)&req_addr, sizeof(client_addr)) < 0) {
		perror("Failed to bind client fd");
		exit(1);
	}*/
	fcntl(client_fd, F_SETFL, fcntl(client_fd, F_GETFL, 0) | O_NONBLOCK);
	buffer_size = max(buffer_size, r.bsize);
		
	ClientHandler * client = new ClientHandler(r, client_fd, *((sockaddr*)&client_addr), this);
	clients.insert(make_pair(client_fd, client));
	client->start();
}

void Handler::removeClient(int fd) {
	delete clients[fd];
	clients.erase(fd);
	IOManager::removeEvent(fd);
	close(fd);
}

void Handler::handleTimeout() {
	for (auto client_it = clients.begin(); client_it != clients.end(); ++client_it) {
		client_it->second->checkTimeout();
	}
}
