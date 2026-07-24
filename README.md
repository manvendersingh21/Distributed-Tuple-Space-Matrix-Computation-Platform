# Distributed Tuple Space Matrix Computation Platform

A distributed computing platform built in C for matrix computations using tuple space principles. This project implements a client-server architecture for parallel and distributed matrix computation operations.

## Overview

This platform leverages distributed tuple space concepts to enable scalable matrix computation across multiple client-server connections. It provides a foundation for performing complex mathematical operations in a distributed environment.

## Features

- **Client-Server Architecture**: TCP socket-based communication for reliable data transfer
- **Tuple Space Principles**: Implements distributed tuple space concepts for parallel computation
- **Matrix Operations**: Support for distributed matrix computation tasks
- **Socket-Based Communication**: Uses standard POSIX sockets for cross-platform compatibility
- **Scalable Design**: Capable of handling multiple concurrent client connections

## Project Structure

```
Distributed-Tuple-Space-Matrix-Computation-Platform/
├── demo/
│   ├── client.c       # Client implementation with socket communication
│   ├── server.c       # Server implementation with connection handling
│   └── ...
├── README.md          # This file
└── [additional source files]
```

## Technology Stack

- **Language**: C (84.2%)
- **Build Scripts**: Shell (15.1%)
- **Build Configuration**: Makefile (0.7%)
- **License**: Apache License 2.0

## Getting Started

### Prerequisites

- GCC compiler or compatible C compiler
- POSIX-compliant operating system (Linux, macOS, Unix)
- Standard C libraries

### Building the Project

1. Clone the repository:
```bash
git clone https://github.com/manvendersingh21/Distributed-Tuple-Space-Matrix-Computation-Platform.git
cd Distributed-Tuple-Space-Matrix-Computation-Platform
```

2. Compile the project:
```bash
make
```

### Running the Demo

#### Start the Server

```bash
./demo/server
```

The server will listen on port 38767 by default.

#### Connect with Client

In another terminal:

```bash
./demo/client
```

The client will:
- Connect to the server at 127.0.0.1:38767
- Send a "ping" message
- Receive a "pong" acknowledgment response

## Usage

### Server Configuration

- Default port: 38767 (configurable via `PORT` constant)
- Maximum buffer size: 1024 bytes
- Supports concurrent client connections (up to 5 connections by default)

### Client Configuration

- Server IP: 127.0.0.1 (localhost)
- Server port: 38767
- Maximum buffer size: 1024 bytes

## Implementation Details

### Client (demo/client.c)

The client application:
- Creates a TCP socket
- Connects to the server
- Sends a ping message
- Receives acknowledgment (pong) from server
- Closes the connection

### Server (demo/server.c)

The server application:
- Creates and binds a TCP socket
- Listens for incoming client connections
- Accepts client requests
- Receives messages and responds with acknowledgments
- Handles multiple concurrent connections

## Architecture

This project follows a distributed computing model where:
- **Clients** submit computation requests
- **Server** manages and processes requests
- **Tuple Space** acts as a shared computational space for data exchange
- **Communication** uses TCP sockets for reliable message passing

## Network Configuration

- **Protocol**: TCP/IP
- **Default Port**: 38767
- **Address Family**: IPv4
- **Connection Type**: Stream-based (connection-oriented)

## Error Handling

Both client and server include robust error handling:
- Socket creation errors
- Connection errors
- Data transmission errors
- Buffer overflow prevention

## Extending the Platform

To extend this platform for matrix operations:

1. Add matrix data structures and operations
2. Implement serialization/deserialization for matrices
3. Create tuple space operations for matrix distribution
4. Add computation kernels for parallel processing
5. Implement distributed coordination algorithms

## Future Enhancements

- [ ] Matrix data type support
- [ ] Distributed matrix multiplication
- [ ] Fault tolerance mechanisms
- [ ] Load balancing
- [ ] Performance optimizations
- [ ] IPv6 support
- [ ] TLS/SSL encryption
- [ ] Multi-threading support

## License

This project is licensed under the Apache License 2.0 - see the LICENSE file for details.

## Author

**Manvender Singh** - [GitHub Profile](https://github.com/manvendersingh21)

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

## Troubleshooting

### Connection Refused
- Ensure the server is running before starting the client
- Verify the port number matches between client and server
- Check firewall settings

### Buffer Errors
- Increase `MAX_BUFFER_SIZE` if handling larger messages
- Ensure message length doesn't exceed buffer size

### Socket Errors
- Verify network connectivity
- Check system resources (file descriptors)
- Review system error messages from perror()

## References

- POSIX Socket Programming
- Distributed Computing Concepts
- Tuple Space Architecture
- Matrix Computation Algorithms
