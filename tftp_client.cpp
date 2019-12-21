// vim:ts=4:sts=4:sw=4:expandtab

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include <boost/program_options.hpp>
#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

namespace po = boost::program_options;

using namespace std;

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

#define TIMEOUT_ALPHA 0.125
#define TIMEOUT_BETA 0.25
#define TIMEOUT_MULTIPLIER 1.2
#define CLOCK_GRANULARITY 100
#define TIMEOUT_K 4

#define USE_TIMEOUT 0

struct rrequest {
	int type;
	int mode;
	string filename;
	vector<string> options;
	int bsize = 512;
	int wsize = 1;
};

class ClientHandler {
	bool error = false;

	rrequest r;

	int fd;
	sockaddr_in addr;
	socklen_t addr_size;
	FILE * f;

	unsigned short block_number;
	unsigned short last_block = 0;

	bool last_packet_sent = false;
	bool last_packet_received = false;

	int current_rto = 1000;
	int srtt;
	int rttvar;
	bool first_measurement = true;
	bool timer_is_set = false;
	chrono::time_point<chrono::system_clock> last_sending_time;

	void updateRTT() {
		auto current_time = chrono::system_clock::now();
		int difference = chrono::duration_cast<chrono::milliseconds>(current_time - last_sending_time).count();
		timer_is_set = false;
		if (first_measurement) {
			first_measurement = false;
			srtt = difference;
			rttvar = difference / 2;
			current_rto = srtt + max(CLOCK_GRANULARITY, TIMEOUT_K * rttvar);
			return;
		}

		rttvar = (1 - TIMEOUT_BETA) * rttvar + TIMEOUT_BETA * abs(srtt - difference);
		srtt = (1 - TIMEOUT_ALPHA) * srtt + TIMEOUT_ALPHA * difference;
		current_rto = srtt + max(CLOCK_GRANULARITY, TIMEOUT_K * rttvar);
		current_rto = max(current_rto, 1000);

		timeval timeout;
		timeout.tv_sec = 0;
    		timeout.tv_usec = TIMEOUT_MULTIPLIER * current_rto * 1000;
    		if (USE_TIMEOUT && setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) < 0) {
	    		perror("setsockopt failed");
	    		error = true;
			return;
    		}
	}

	void _sendRequest() {
		int filename_len = r.filename.length();
		string mode = (r.mode == NETASCII) ? "netascii" : "octet";
		int mode_len = mode.length();
		int bytes_length = 2 + (filename_len + 1) + (mode_len + 1);

		string str_bsize;
		int blksize_len;
		string blksize_title;
		int blksize_title_len;
		if (find(r.options.begin(), r.options.end(), BLKSIZE) != r.options.end()) {
			str_bsize = to_string(r.bsize);
			blksize_len = str_bsize.length();
			blksize_title = BLKSIZE;
			blksize_title_len = blksize_title.length();
			bytes_length += blksize_len + blksize_title_len + 2;
		}

		string str_wsize;
		int wdwsize_len;
		string wdwsize_title;
		int wdwsize_title_len;
		if (find(r.options.begin(), r.options.end(), WINDOWSIZE) != r.options.end()) {
			str_wsize = to_string(r.wsize);
			wdwsize_len = str_wsize.length();
			wdwsize_title = WINDOWSIZE;
			wdwsize_title_len = wdwsize_title.length();
			bytes_length += wdwsize_len + wdwsize_title_len + 2;
		}

		unsigned char bytes[bytes_length];
		unsigned char * current_byte = bytes;
		*current_byte = 0; ++current_byte;
		*current_byte = r.type; ++current_byte;
		for (int i = 0; i < filename_len; ++i) {
			*current_byte = r.filename[i];
			++current_byte;
		}
		*current_byte = 0; ++current_byte;
		for (int i = 0; i < mode_len; ++i) {
			*current_byte = mode[i];
			++current_byte;
		}
		*current_byte = 0; ++current_byte;
		if (find(r.options.begin(), r.options.end(), BLKSIZE) != r.options.end()) {
			for (int i = 0; i < blksize_title_len; ++i) {
				*current_byte = blksize_title[i];
				++current_byte;
			}
			*current_byte = 0; ++current_byte;
			for (int i = 0; i < blksize_len; ++i) {
				*current_byte = str_bsize[i];
				++current_byte;
			}
			*current_byte = 0; ++current_byte;
		}
		if (find(r.options.begin(), r.options.end(), WINDOWSIZE) != r.options.end()) {
			for (int i = 0; i < wdwsize_title_len; ++i) {
				*current_byte = wdwsize_title[i];
				++current_byte;
			}
			*current_byte = 0; ++current_byte;
			for (int i = 0; i < wdwsize_len; ++i) {
				*current_byte = str_wsize[i];
				++current_byte;
			}
			*current_byte = 0; ++current_byte;
		}
		
		if (!timer_is_set) {
			timer_is_set = true;
			last_sending_time = chrono::system_clock::now();
		}

		sendto(fd, bytes, bytes_length, 0, (sockaddr*)&addr, addr_size);
	}

	bool _receiveOACK() {
		int buf_size = max(r.bsize + 4, 25);
		char bytes[buf_size];
		int nread = recvfrom(fd, bytes, buf_size, 0, (sockaddr*)&addr, &addr_size);
		updateRTT();
		if (bytes[0] != 0) return false;
		if (bytes[1] == OACK) {
			cout << "client: Received OACK" << endl;
			char * zero = (char*) memchr(bytes + 2, 0, nread - 2);
			if (zero == nullptr) {
				cout << "OACK packet: bad format" << endl;
				return false;
			}
			zero = bytes + 2;
			while (zero != bytes + (nread - 1)) {
				zero = (char*) memchr(zero + 1, 0, nread - (zero-bytes) - 1);
				if (zero == nullptr) {
					cout << "OACK packet: bad format" << endl;
					return false;
				}
				string blksize = BLKSIZE;
				string wdwsize = WINDOWSIZE;
				if (!strncmp(zero - blksize.length(), blksize.c_str(), blksize.length())) {
					if (find(r.options.begin(), r.options.end(), blksize) == r.options.end()) {
						cout << "OACK packet: bad option" << endl;
						return false;
					}
					if (zero == bytes + (nread - 1)) {
						cout << "OACK packet: bad format" << endl;
						return false;
					}
					r.bsize = strtol(zero+1, NULL, 10);
					zero = (char*) memchr(zero + 1, 0, nread - (zero-bytes) - 1);
					if (zero == nullptr) {
						cout << "OACK packet: bad format" << endl;
						return false;
					}
				} else if (!strncmp(zero - wdwsize.length(), wdwsize.c_str(), wdwsize.length())) {
					if (find(r.options.begin(), r.options.end(), wdwsize) == r.options.end()) {
						cout << "OACK packet: bad option" << endl;
						return false;
					}
					if (zero == bytes + (nread - 1)) {
						cout << "OACK packet: bad format" << endl;
						return false;
					}
					r.wsize = strtol(zero+1, NULL, 10);
					zero = (char*) memchr(zero + 1, 0, nread - (zero-bytes) - 1);
					if (zero == nullptr) {
						cout << "OACK packet: bad format" << endl;
						return false;
					}	
				} else;  // other options
			}
			return true;
		} else {
			cout << "client: didn't received OACK - continue with default options" << endl;
			r.bsize = 512;
			if (bytes[1] == ACK && r.type == WRQ) {
				if (nread == 4) {
					unsigned short ackled_block = (bytes[2] << 8) + bytes[3];
					if (ackled_block != 0) {
						cout << "ACK packet: bad tid" << endl;
						return false;
					}
					last_block = 0;
					block_number = 1;
					return true;
				} else {
					cout << "ACK packet: bad size" << endl;
					return false;
				}
			} else if (bytes[1] == DATA && r.type == RRQ) {
				for (int i = 0; i < r.wsize; ++i) {
					if (nread >= 2) {
						unsigned short recvd_block = (bytes[2] << 8) + bytes[3];
						if (last_block + 1 == recvd_block) last_block = recvd_block;
						else {
							cout << "DATA packet: bad tid" << endl;
							return false;
						}
						if (nread < r.bsize + 4) {
							last_packet_received = last_packet_sent = true;
							break;
						}
					} else {
						cout << "DATA packet: bad size" << endl;
						return false;
					}
				}
				block_number = last_block;
				return true;
			} else return false;
		}
	}

	void _receiveACK() {
		char buf[4] {};
		auto size = recvfrom(fd, buf, 4, 0, (sockaddr*)&addr, &addr_size);
		if (size < 0) {
			return;
		}
		updateRTT();
		if (size == 4 && buf[0] == 0 && buf[1] == ACK) {
			unsigned short ackled_block = (buf[2] << 8) + buf[3];
			if ((ackled_block == 0) || (last_block + r.wsize >= ackled_block && last_block < ackled_block)) {
				last_block = ackled_block;
				block_number = ackled_block + 1;
			} else {
				//TODO sth went wrong
				error = true;
				return;
			}
			if (last_packet_sent) {
				last_packet_received = true;
			}
		} else {
			error = true;
		}
	}

	void _sendACK() {
		char buf[4];
		buf[0] = 0;
		buf[1] = ACK;
		buf[2] = (block_number >> 8) & 255;
		buf[3] = block_number & 255;

		if (!timer_is_set) {
			timer_is_set = true;
			last_sending_time = chrono::system_clock::now();
		}

		sendto(fd, buf, 4, 0, (sockaddr*)&addr, addr_size);
	}

	void _receiveData() {
		for (int i = 0; i < r.wsize; ++i) {
			char buf[r.bsize+4] {};
			int size = recvfrom(fd, buf, r.bsize + 4, 0, (sockaddr*)&addr, &addr_size);
			if (size < 0) {
				return;
			}
			updateRTT();
			if (size >= 2 && buf[0] == 0 && buf[1] == DATA) {
				unsigned short recvd_block = (buf[2] << 8) + buf[3];
				if (last_block + 1 == recvd_block) last_block = recvd_block;
				else {
					--i;
					continue;
				}
				fwrite(buf + 4, sizeof(char), size - 4, f);
				if (size < r.bsize + 4) {
					last_packet_received = last_packet_sent = true;
					break;
				}	
			} else {
				cout << "client: DATA packet: bad format" << endl;
				error = true;
				return;
			}
		}
		block_number = last_block;
	}

	void _sendData() {
		for (int i = 0; i < r.wsize; ++i) {
			char buf[r.bsize + 4] {};
			buf[0] = 0;
			buf[1] = DATA;
			buf[2] = (block_number >> 8) & 255;
			buf[3] = block_number & 255;
			fseek(f, r.bsize*(block_number - 1), SEEK_SET);
			int size = fread(buf + 4, sizeof(char), r.bsize, f);
			if (size < 0) {
				cout << "Error while reading from file " << r.filename << endl;
				error = true;
				return;
			}

			if (!timer_is_set) {
				timer_is_set = true;
				last_sending_time = chrono::system_clock::now();
			}

			sendto(fd, buf, size + 4, 0, (sockaddr*)&addr, addr_size);
			cout << "client: Sent DATA " << block_number << endl;
			if (size < r.bsize) {
				last_packet_sent = true;
				break;
			}
			++block_number;
		}
	}

public:
	ClientHandler(rrequest & _r, string & local_filename, int port, string ip_addr) {
		r = _r;
		fd = socket(AF_INET, SOCK_DGRAM, 0);
		addr_size = sizeof(addr);
		memset(&addr, 0, addr_size);
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		inet_aton(ip_addr.c_str(), &addr.sin_addr);
		//fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

		char mode[2] {};
		mode[0] = (r.type == RRQ) ? 'w' : 'r';
		mode[1] = (r.mode == NETASCII) ? 't' : 'b';
		f = fopen(local_filename.c_str(), mode);

		block_number = 1;
		last_block = 0;
	}
	~ClientHandler() { fclose(f); close(fd); }

	bool request() {
		_sendRequest();
		if (r.options.size() == 0) {
			if (r.type == RRQ) _receiveData();
			else _receiveACK();
			return true;
		}
		if (r.type == RRQ) block_number = 0;
		return _receiveOACK();
	}
	
	void handle() {
		if (r.type == RRQ) {
			while (!error) {
				_sendACK();
				if (last_packet_received) return;
				_receiveData();
			}
		} else {
			while (!error) {
				if (last_packet_sent) return;
				_sendData();
				_receiveACK();
				//break;
			}
		}
		if (error) {
			cout << "client: Fatal error - aborting communication" << endl;
		}	
	}
};

int main(int argc, char** argv) {
    int PORT = 6969;
    string ip_addr = "40.87.143.114";
    rrequest r;
    string local_filename;

    po::options_description desc("Allowed options");
    desc.add_options()
	    ("help", "produce help message")
	    ("port", po::value<int>(), "port")
	    ("ip", po::value<string>(), "ip")
	    ("r", "read from remote server")
	    ("w", "write to remote server")
	    ("local", po::value<string>(), "local filename")
	    ("remote", po::value<string>(), "remote filename")
	    ("mode", po::value<string>(), "mode (netascii/octet)")
	    ("blocksize", po::value<int>(), "single datagram size")
	    ("windowsize", po::value<int>(), "size of DATA packets' window");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if(vm.count("help")) {
	    cout << desc << endl;
	    return 1;
    }

    if (vm.count("port")) {
	    PORT = vm["port"].as<int>();
    }
    if (vm.count("ip")) {
	    ip_addr = vm["ip"].as<string>();
    }
    if ((vm.count("r") && vm.count("w")) || (!vm.count("r") && !vm.count("w"))) {
	    cout << "It is not clear whether this request is read or write!" << endl;
	    return 1;
    }
    if (vm.count("r")) r.type = RRQ;
    else r.type = WRQ;
    if (vm.count("remote")) {
	    r.filename = vm["remote"].as<std::string>();
    } else {
	    if (r.type == RRQ) {
	    	std::cout << "File name must be specified!" << std::endl;
	    	return 1;
	    } else {
		if (vm.count("local")) r.filename = vm["local"].as<string>();
		else {
			cout << "Either local or remote file must be specified" << endl;
			return 1;
		}
	    }
    }
    if (vm.count("local")) {
	    local_filename = vm["local"].as<string>();
    } else {
	    if (r.type == WRQ) {
		    cout << "File name must be specified!" << endl;
		    return 1;
	    } else {
	    	local_filename = r.filename;
	    }
    }
    if (vm.count("mode")) {
	    r.mode = (vm["mode"].as<string>()[0] == 'n') ? NETASCII : OCTET;
    } else {
	    cout << "Mode must be specified!" << std::endl;
	    return 1;
    }
    if (vm.count("blocksize")) {
	    r.options.push_back(BLKSIZE);
	    r.bsize = vm["blocksize"].as<int>();
    }
    if (vm.count("windowsize")) {
	    r.options.push_back(WINDOWSIZE);
	    r.wsize = vm["windowsize"].as<int>();
    }

    ClientHandler handler(r, local_filename, PORT, ip_addr);
    if (handler.request()) {
	handler.handle();
    } else {
	cout << "server rejected the request" << endl;
	return 1;
    }

    return 0;
}
