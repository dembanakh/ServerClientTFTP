#include "IOManager.h"

class IOManager;

int main(int argc, char ** argv) {
	bool oneport = false;
	if (argc > 1 && !strcmp(argv[1], "oneport")) oneport = true;

	IOManager io(oneport);
	io.start();

	return 0;
}
