g++ -Wall -o server tftp_server.cpp config.cpp IOManager.cpp IHandler.cpp Handler.cpp HandlerOnePort.cpp ClientHandler.cpp ClientHandlerOnePort.cpp
g++ -Wall -o client tftp_client.cpp -lboost_program_options
