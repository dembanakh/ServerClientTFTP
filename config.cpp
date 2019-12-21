#include "config.h"

string to_key(sockaddr_in * addr) {
	char buf[16] {};
	inet_ntop(AF_INET, &(addr->sin_addr), buf, INET_ADDRSTRLEN);
	string key(buf);
	key += ':';
	key += to_string(ntohs(addr->sin_port));
	//cout << key << endl;
	return key;
}
