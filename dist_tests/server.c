#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(void) {
    // struct addrinfo hints;
	// struct addrinfo *result, *rp;
	// int sock_fd;
	// char* server_port = "4444";
	// int server_port_num = 4444;
	// int error = 0;
	// int res;
	// int opt = 1;

	// int ip_protocol = AF_INET; // IPv4
	// int transport_protocol = SOCK_STREAM; // TCP

	// memset(&hints, 0, sizeof(hints));
	// hints.ai_family = ip_protocol; // use IPv4
	// hints.ai_socktype = transport_protocol; // use TCP
	// hints.ai_protocol = 0; // any protocol
    
    // int accept_socket;

	// printf("opening connections to client\n");
	
	// // set up a socket to listen for a connection request
	// // at some point, this will have to run in the background
	// // so that the server can do other work while also listening
	// // for new requests
	// sock_fd = socket(ip_protocol, transport_protocol, 0);
	// if (sock_fd < 0) {
	// 	printf("socket failed: %s\n", strerror(errno));
	// 	// return sock_fd;
	// 	assert(0);
	// }

	// printf("got socket %d\n", sock_fd);

	// memset(&hints, 0, sizeof(hints));
	// hints.ai_family = AF_INET;
	// hints.ai_socktype = SOCK_STREAM;
	// hints.ai_flags = AI_PASSIVE;
	// hints.ai_protocol = 0;
	// hints.ai_canonname = NULL;
	// hints.ai_addr = NULL;
	// hints.ai_next = NULL;

	// res = getaddrinfo(NULL, server_port, &hints, &result);
	// if (res < 0) {
	// 	printf("getaddrinfo\n");
	// 	assert(0);
	// }
	
	// sock_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	// if (sock_fd < 0) {
	// 	printf("socket\n");
	// 	assert(0);
	// }

	// printf("setting socket options\n");

	// // allow socket to be reused to avoid problems with binding in the future
	// res = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	// if (res < 0) {
	// 	printf("setsockopt failed: %s\n", strerror(errno));
	// 	// return res;
	// 	assert(0);
	// }

	// // bind the socket to the local address and port so we can accept connections on it
	// // res = bind(sock_fd, (struct sockaddr*)&my_addr, addrlen);
	// res = bind(sock_fd, result->ai_addr, result->ai_addrlen);
	// if (res < 0) {
	// 	printf("bind failed: %s\n", strerror(errno));
	// 	// return ret;
	// 	assert(0);
	// }

	// printf("listening for connections\n");
	// // set up socket to listen for connections
	// res = listen(sock_fd, 2);
	// if (res < 0) {
	// 	// perror("listen");
	// 	printf("listen failed: %s\n", strerror(errno));
	// 	// return ret;
	// 	assert(0);
	// }

	// // wait for someone to connect and accept when they do
	// printf("waiting for connections\n");
	// accept_socket = accept(sock_fd, result->ai_addr, &result->ai_addrlen);
	// if (accept_socket < 0) {
	// 	printf("accept failed: %s\n", strerror(errno));
	// 	// return accept_socket;
	// 	assert(0);
	// }

	// freeaddrinfo(result);

	// // TODO: close these later when we won't need them anymore
	// close(sock_fd);
	// close(accept_socket);


    return 0;
}