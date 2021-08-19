
# ServerClientTFTP

Object-Oriented implementation of Server and Client under TFTP protocol (RFC1350) in C++ for Computer Networks course. Allows one to exchange files between server machine and client machine.

# How To Build

- `make client`
- `make server`
- `make all`
- `make clean`

# How To Use

- `./client --help` for more information
- `./client` with the following options for executing
- specify --port and --ip of server application
- use --r to read the file from server
- use --w to write the file to server
- specify --local and --remote names of the file
- specify --mode of the transfer (netascii or octet)
- optionally, specify --blocksize (size of every datagram - default 512)
- optionally, specify --windowsize (default 1)
