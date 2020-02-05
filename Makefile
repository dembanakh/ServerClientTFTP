CC = g++

all: server client


server: tftp_server.cpp config.cpp IOManager.cpp IHandler.cpp Handler.cpp HandlerOnePort.cpp ClientHandler.cpp ClientHandlerOnePort.cpp IOManager.h IHandler.h Handler.h HandlerOnePort.h ClientHandler.h ClientHandlerOnePort.h config.cpp config.h IClientHandler.h
	$(CC) -Wall -o server tftp_server.cpp config.cpp IOManager.cpp IHandler.cpp Handler.cpp HandlerOnePort.cpp ClientHandler.cpp ClientHandlerOnePort.cpp

client: tftp_client.cpp
	$(CC) -Wall -o client tftp_client.cpp -lboost_program_options

clean:
	rm client server
