#pragma once

#include "config.h"
#include "IHandler.h"
#include "IOManager.h"

class IHandler;
class IOManager;

class IClientHandler {
protected:
	IHandler * handler;

	bool error = false;

	request r;
	int fd;
	sockaddr addr;
	socklen_t addr_size;
	FILE * f;

	unsigned short subwindow_bnum = 0;
	unsigned short block_number;
	unsigned short last_block = 0;

	bool last_packet_sent = false;
	bool last_packet_received = false;

	bool oack_received = false;

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
	}

	virtual void removeSelf() {};

	void sendACK() {
		char buf[4];
		buf[0] = 0;
		buf[1] = ACK;
		buf[2] = (block_number >> 8) & 255;
		buf[3] = block_number & 255;
		
		if (!timer_is_set) {
			timer_is_set = true;
			last_sending_time = chrono::system_clock::now();
		}
		
		sendto(fd, buf, 4, 0, &addr, addr_size);
		cout << "Sent ACK block " << block_number << endl;
		if (last_packet_sent) {
			cout << "Communication finished" << endl;
			fclose(f);
			removeSelf();
		} else IOManager::modifyEvent(fd, EPOLLIN | 0); // EPOLLET
	}

	void sendData() {
		for (int i = 0; i < r.wsize; ++i) {
			char buf[r.bsize + 4] {};
			buf[0] = 0;
			buf[1] = DATA;
			buf[2] = (block_number >> 8) & 255;
			buf[3] = block_number & 255;
			fseek(f, r.bsize*(block_number - 1), SEEK_SET);
			int size = fread(buf + 4, sizeof(char), r.bsize, f);
			if (size < 0) {
				perror("Error while read from file");
				error = true;
				return;	
			}
				
			if (!timer_is_set) {
				timer_is_set = false;
				last_sending_time = chrono::system_clock::now();
			}
		
			sendto(fd, buf, size + 4, 0, &addr, addr_size);
			cout << "server: Sent DATA block " << block_number << endl;
			if (size < r.bsize) {
				last_packet_sent = true;
				break;
			}	
			++block_number;
		}
		IOManager::modifyEvent(fd, EPOLLIN | 0); // EPOLLET
	}

	void receiveData(char * buf, ssize_t buf_size) {
		updateRTT();
		if (buf_size >= 2) {
			unsigned short recvd_block = (buf[2] << 8) + buf[3];
			cout << "Received DATA block " << recvd_block << endl;
			if (last_block + 1 == recvd_block) {
				last_block = recvd_block;
				if (!oack_received && last_block == 1) oack_received = true;
			}
			else {
				subwindow_bnum = r.wsize;
				return;
			}
			fwrite(buf + 4, sizeof(char), buf_size - 4, f);
			if (buf_size < r.bsize + 4) {
				last_packet_received = last_packet_sent = true;
				subwindow_bnum = r.wsize;
				return;
			}
		} else {
			error = true;
			return;
		}
		++subwindow_bnum;
	}

	void receiveACK(char * buf, ssize_t buf_size) {
		updateRTT();
		if (buf_size == 4) {
			unsigned short ackled_block = (buf[2] << 8) + buf[3];
			cout << "Received ACK block " << ackled_block << endl;
			if ((r.type == RRQ && last_block == 65535 && ackled_block==0)||
					(last_block + r.wsize >= ackled_block && last_block < ackled_block)) {
				last_block = ackled_block;
				block_number = ackled_block + 1;
				if (last_block == 65535) oack_received = true; // ACK0
			} else {
				//sth went wrong
				cout << "last_block=" << last_block << endl;
				cout << "Error: IClientHandler::receiveACK() ackled_block=" << ackled_block << endl;
				error = true;
				return;
			}
			if (last_packet_sent) {
				last_packet_received = true;
				cout << "Communication finished" << endl;
				fclose(f);
				removeSelf();
			}
		} else {
			cout << "Error: IClientHandler::receiveACK() buf_size=" << buf_size << endl;
			error = true;
			return;
		}
	}

	bool sendOACK() {
		if (r.options.size() == 0) return false;

		int buf_size = 2;
		for (string option : r.options) {
			buf_size += option.length();
			buf_size += 1;
			if (option == BLKSIZE) buf_size += to_string(r.bsize).length() + 1;
			else if (option == WINDOWSIZE) buf_size += to_string(r.wsize).length() + 1; 
			else;// other options
		}
		char buf[buf_size] {};
		buf[0] = 0;
		buf[1] = OACK;
		int i = 2;
		for (string option : r.options) {
			for (char c : option) buf[i++] = c;
			buf[i++] = 0;
			if (option == BLKSIZE)
				for (char c : to_string(r.bsize)) buf[i++] = c;
			else if (option == WINDOWSIZE)
				for (char c : to_string(r.wsize)) buf[i++] = c; 
			else; // other options
			buf[i++] = 0;
		}

		if (!timer_is_set) {
			timer_is_set = true;
			last_sending_time = chrono::system_clock::now();
		}

		sendto(fd, buf, buf_size, 0, &addr, addr_size);
		cout << "server: Sent OACK" << endl;
		IOManager::modifyEvent(fd, EPOLLIN | 0); // EPOLLET
		return true;
	}

public:
	virtual ~IClientHandler() {}
	void checkTimeout() {	
		auto current_time = chrono::system_clock::now();
		int difference = chrono::duration_cast<chrono::milliseconds>(current_time - last_sending_time).count();

		if (difference > TIMEOUT_MULTIPLIER * current_rto) {
			if (difference > 10 * TIMEOUT_MULTIPLIER * current_rto) {
				// obviously dead client
				cout << "server: Client doesn't answering - retransmission" << endl;
				fclose(f);
				removeSelf();
			}

			if (!oack_received) {
				sendOACK();
				return;
			}

			if (r.type == WRQ) sendACK();
			else if (!last_packet_sent) sendData();	
		}
	}

	void notify(int opcode, char * buf, ssize_t buf_size) {
		if (r.type == WRQ) {
			if (opcode == DATA) {
				receiveData(buf, buf_size);
				if (subwindow_bnum == r.wsize) {
					subwindow_bnum = 0;
					block_number = last_block;
					if (!error) sendACK();
				}
			} else cout << "Error: IClientHandler::notify() opcode" << endl; //error
		} else {
			if (opcode == ACK) {
				receiveACK(buf, buf_size);
				if (!last_packet_sent && !error) sendData();
			} else cout << "Error: IClientHandler::notify() opcode" << endl; //error
		}
		if (error) {
			cout << "server: Fatal error - aborting communication" << endl;
			fclose(f);
			removeSelf();
		}
	}

	virtual void start() {};
};
