#include "config.h"
#include "HandlerOnePort.h"
#include "ClientHandlerOnePort.h"

class ClientHandlerOnePort;

void HandlerOnePort::handle(int _fd, bool in) {
	if (_fd != fd) return;
	if (in) {
		sockaddr_in income;
		socklen_t income_size;
		char buf[buffer_size] {};
		int s = recvfrom(fd, buf, buffer_size, 0, (sockaddr*)&income, &income_size);
		
		string key = to_key(&income);
		auto cl = clients.find(key);
		if (cl == clients.end()) {
			if (IS_RRQ(buf)) acceptNewClient(RRQ, buf, s, &income, &income_size);
			else if (IS_WRQ(buf)) acceptNewClient(WRQ, buf, s, &income, &income_size);
			else return; //rubbish in fd
		} else {
			if (IS_DATA(buf)) cl->second->notify(DATA, buf, s);
			else if (IS_ACK(buf)) cl->second->notify(ACK, buf, s);
			else return; //rubbish in fd
		}
	}
}

void HandlerOnePort::acceptNewClient(int req_type, char * buf, int buf_size, sockaddr_in * req_addr, socklen_t * req_size) {
	request r;
	r.type = req_type;
	cout << "Received " << (req_type == RRQ ? "Read" : "Write") << " request" << endl;
	if (!receiveRequest(buf, buf_size, &r)) return;

	buffer_size = max(buffer_size, r.bsize);

	ClientHandlerOnePort * client = new ClientHandlerOnePort(r, fd, req_addr, this);
	clients.insert(make_pair(to_key(req_addr), client));
	client->start();
}

void HandlerOnePort::removeClient(sockaddr * addr) {
	string key = to_key((sockaddr_in*)addr);
	delete clients[key];
	clients.erase(key);
}

void HandlerOnePort::handleTimeout() {
	for (auto client_it = clients.begin(); client_it != clients.end(); ++client_it) {
		client_it->second->checkTimeout();
	}
}


