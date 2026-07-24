// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1" // Assume local server
#define PORT 38767             // Replace with your TUIDXXXXX (< 65,000)
#define MAX_BUFFER_SIZE 1024  // Maximal comm. buffer size

void error(const char *msg) {
    perror(msg);  // OS error handler
    exit(1);	  // exit as Failure
}

int main() {
    int sockfd, n;  // Client only needs a single socket
    struct sockaddr_in server_addr; // Client only need a single server address
    char buffer[MAX_BUFFER_SIZE];

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    printf("%d",sockfd);
    if (sockfd < 0) {
        error("ERROR opening socket");
    }

    // Prepare server address
    memset((char *)&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(PORT);

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        error("ERROR connecting");
    }

    // Send "ping" message to server
    strcpy(buffer, "Client sending: ping");
    n = write(sockfd, buffer, strlen(buffer));
    if (n < 0) {
        error("ERROR writing to socket");
    }

    // Receive Ack from server
    memset(buffer, 0, MAX_BUFFER_SIZE);  // Clear the buffer
    n = read(sockfd, buffer, MAX_BUFFER_SIZE - 1);
    if (n < 0) {
        error("ERROR reading from socket");
    }
    printf("Received from server: (%s)\n", buffer);

    close(sockfd);
    return 0;
}

