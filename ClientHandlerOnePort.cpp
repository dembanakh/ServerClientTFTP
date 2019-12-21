#include "ClientHandlerOnePort.h"
#include "HandlerOnePort.h"

class HandlerOnePort;

void ClientHandlerOnePort::removeSelf() { ((HandlerOnePort*)handler)->removeClient(&addr); }
