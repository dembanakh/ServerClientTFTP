#include "ClientHandler.h"
#include "Handler.h"

class Handler;

void ClientHandler::removeSelf() { ((Handler*)handler)->removeClient(fd); }
