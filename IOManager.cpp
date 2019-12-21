#include "IOManager.h"
#include "IHandler.h"
#include "HandlerOnePort.h"
#include "Handler.h"

class IHandler;
class HandlerOnePort;
class Handler;

IOManager::IOManager(bool oneport) {
	epoll_fd = epoll_create1(0);
	if (epoll_fd < 0) {
		perror("Failed to create epoll");
		exit(1);
	}

	events = new epoll_event[MAX_EVENTS] {};
	if (oneport) handler = new HandlerOnePort(this);
	else         handler = new Handler(this);
}

void IOManager::start() {
	while (true) {
		int num_fds = epoll_wait(IOManager::epoll_fd, this->events, this->MAX_EVENTS, 5);
		
		for (int i = 0; i < num_fds; ++i) {
				int fd = events[i].data.fd;
				handler->handle(fd, events[i].events & EPOLLIN);
		}
		handler->handleTimeout();
	}
}

int IOManager::epoll_fd = 0;
