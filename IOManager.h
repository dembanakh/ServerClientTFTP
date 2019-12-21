#pragma once

#include "IHandler.h"
#include "config.h"

class IHandler;

class IOManager {
	int MAX_EVENTS = MAX_TID;

	static int epoll_fd;
	struct epoll_event * events;
	IHandler * handler;
public:
	IOManager(bool);
	~IOManager() { delete[] events; /*delete handler;*/ }
	void start();


	static void addEvent(int fd, unsigned int flag) {
		epoll_event e;
		e.data.fd = fd;
		e.events = flag;
		if (epoll_ctl(IOManager::epoll_fd, EPOLL_CTL_ADD, fd, &e) < 0) {
			perror("Failed to add epoll_event");
		}
	}

	static void modifyEvent(int fd, unsigned int flag) {
		epoll_event e;
		e.data.fd = fd;
		e.events = flag;
		if (epoll_ctl(IOManager::epoll_fd, EPOLL_CTL_MOD, fd, &e) < 0) {
			perror("Failed to modify epoll_event");
		}
	}

	static void removeEvent(int fd) {
		epoll_event e;
		e.data.fd = fd;
		if (epoll_ctl(IOManager::epoll_fd, EPOLL_CTL_DEL, fd, &e) < 0) {
			perror("Failed to delete epoll_event");
		}
	}
};
