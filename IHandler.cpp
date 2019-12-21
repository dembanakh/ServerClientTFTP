#include "config.h"
#include "IHandler.h"
#include "IOManager.h"

class IOManager;

void IHandler::init() {	
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Failed to create listening fd");
		exit(1);
	}
	addr.sin_family = AF_INET;
	addr.sin_port = htons(REQUEST_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
	addr_size = sizeof(addr);
	if (bind(fd, (sockaddr*)&addr, addr_size) < 0) {
		perror("Failed to bind listening fd");
		exit(1);
	}
	IOManager::addEvent(fd, EPOLLIN);
}

bool IHandler::receiveRequest(char * buf, int size, request * r) {
	char * zero = (char*) memchr(buf+2, 0, size-2);
	if (zero == nullptr) {
		cout << "server: rq error (zero == nullptr in Handler::receiveRequest())" << endl; //error
		return false;
	}
	r->filename = string(buf+2, (zero - buf) - 2);
	cout << "request for file " << r->filename << endl;
	zero = (char*) memchr(zero+1, 0, size - (zero-buf) - 1);
	if (zero == nullptr) {
		cout << "server: rq error (zero == nullptr in Handler::receiveRequest())" << endl; //error
		return false;
	}
	if (*(zero-1) == 'i') {
		r->mode = NETASCII;
		cout << "using NETASCII\n";
	} else if (*(zero-1) == 't') {
		r->mode = OCTET;
		cout << "using OCTET\n";
	} else return false;
	while (zero != buf + (size - 1)) {  //options
		zero = (char*) memchr(zero+1, 0, size - (zero-buf) - 1);
		if (zero == nullptr) {
			cout << "rqoptions error" << endl;
			return false;
		}
		string blksize = BLKSIZE;
		string wdwsize = WINDOWSIZE;
		if (!strncmp(zero - blksize.length(), blksize.c_str(), blksize.length())) {
			cout << "requesting for blocksize";
			r->options.push_back(BLKSIZE);
			if (zero == buf + (size - 1)) {
				cout << "rqoptions error" << endl;
				return false;
			}
			r->bsize = strtol(zero+1, NULL, 10);
			cout << " of " << r->bsize << endl;
			if (r->bsize < 8 || r->bsize > 65464) {
				cout << "blksize error" << endl;
				return false;
			}
			zero = (char*) memchr(zero+1, 0, size - (zero-buf) - 1);
			if (zero == nullptr) {
				cout << "rqoptions error" << endl;
				return false;
			}
		} else if (!strncmp(zero - wdwsize.length(), wdwsize.c_str(), wdwsize.length())) {
			cout << "requesting for windowsize";
			r->options.push_back(WINDOWSIZE);
			if (zero == buf + (size - 1)) {
				cout << "rqoptions error" << endl;
				return false;
			}
			r->wsize = strtol(zero+1, NULL, 10);
			cout << " of " << r->wsize << endl;
			if (r->wsize < 1 || r->wsize > 65535) {
				cout << "windowsize error" << endl;
				return false;
			}
			zero = (char*) memchr(zero+1, 0, size - (zero-buf) - 1);
			if (zero == nullptr) {
				cout << "rqoptions error" << endl;
				return false;
			}
		} else; // other options	
	}
	return true;
}
