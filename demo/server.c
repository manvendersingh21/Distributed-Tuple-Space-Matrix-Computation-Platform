// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 38767  // Change this to your TUIDXXXXX (must be < 65000)
#define MAX_BUFFER_SIZE 1024 // Maximal number of bytes to communicate

void error(const char *msg) {
    perror(msg); // OS error message handler
    exit(1); // Report Error
}

int main() {
    int sockfd, newsockfd, n; // Two socket ids for receiving
    socklen_t client_len;     // client address length
    char buffer[MAX_BUFFER_SIZE]; // Communication buffer
    struct sockaddr_in server_addr, client_addr; // Inet address structures

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd < 0) {
        error("ERROR opening socket");
    }

    // Prepare server address
    memset((char *)&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket to address
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        error("ERROR on binding");
    }

    // Listen for incoming connections
    listen(sockfd, 5);  // Wait for at most 5 concurrent connections
    client_len = sizeof(client_addr);

    // Accept client connection
    newsockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len); // Server action
    if (newsockfd < 0) {
        error("ERROR on accept");
    }

    // Communication loop
    while (1) {
        memset(buffer, 0, MAX_BUFFER_SIZE);
        n = read(newsockfd, buffer, MAX_BUFFER_SIZE - 1);
        if (n <= 0) {
            error("Nothing from socket");
        }
        printf("Server Received message: (%s)\n", buffer);

        // Respond with "pong" Ack
        n = write(newsockfd, "Server Message: pong", 20);
        if (n < 0) {
            error("ERROR writing to socket");
        }
    }
	// Need to close both sockets
    close(newsockfd);
    close(sockfd);
    return 0;
}
