#pragma once

#define REQUEST_PORT 6969  // 69 is in use
#define MAX_TID 65536
#define RRQ 1
#define WRQ 2
#define DATA 3
#define ACK 4
#define ERROR 5
#define OACK 6
#define NETASCII 1
#define OCTET 2

#define BLKSIZE "blksize"
#define WINDOWSIZE "windowsize"

#define IS_RRQ(buf) (buf[0] == 0 && buf[1] == RRQ)
#define IS_WRQ(buf) (buf[0] == 0 && buf[1] == WRQ)
#define IS_DATA(buf) (buf[0] == 0 && buf[1] == DATA)
#define IS_ACK(buf) (buf[0] == 0 && buf[1] == ACK)

#define TIMEOUT_ALPHA 0.125
#define TIMEOUT_BETA 0.25
#define TIMEOUT_MULTIPLIER 1.2
#define CLOCK_GRANULARITY 100
#define TIMEOUT_K 4


#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <map>
#include <vector>

using namespace std;

struct request {
	int type;
	int mode;
	string filename;
	vector<string> options;
	int bsize = 512;
	int wsize = 1;
};

string to_key(sockaddr_in*);
