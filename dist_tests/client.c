#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(void) {
    struct addrinfo hints;
	// struct addrinfo *result, *rp;
	// int sock_fd;
	// char* server_port = "4444";
	// int error = 0;
	// int res;
	// int opt = 1;
	// long flags = 0;

	// int ip_protocol = AF_INET; // IPv4
	// int transport_protocol = SOCK_STREAM; // TCP

	// memset(&hints, 0, sizeof(hints));
	// hints.ai_family = ip_protocol; // use IPv4
	// hints.ai_socktype = transport_protocol; // use TCP
	// hints.ai_protocol = 0; // any protocol
    // hints.ai_flags = 0;

	// printf("connecting to server\n");

	// // TODO: get server IP (and port?) in some other way
	// // probably look them up from a configuration file
	// char* server_ip = "10.56.0.253";
	
	// // getaddrinfo is a system call that, given an IP address, a port,
	// // and some additional info about the desired connection, 
	// // returns structure(s) that can be used to establish network 
	// // connections with another machine
	// printf("calling get addr info\n");
	// res = getaddrinfo(server_ip, server_port, &hints, &result);
	// if (res != 0) {
	// 	printf("getaddrinfo failed: %s\n", strerror(errno));
	// 	// return res;
	// 	assert(0);
	// }
	// printf("get addr info done\n");

	// char addrstr[100];
	// inet_ntop(result->ai_family, &((struct sockaddr_in *) result->ai_addr)->sin_addr, addrstr, 100);
	// printf("address: %s\n", addrstr);
	
	// // for (rp = result; rp != NULL; rp = rp->ai_next) {
	// // 	printf("looping\n");
	// 	// socket() creates a network endpoint and returns a file descriptor
	// 	// that can be used to access that endpoint
	// printf("grabbing socket\n");
	// sock_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	// printf("got socket\n");
	// if (sock_fd < 0) {
	// 	printf("bad socket\n");
	// 	assert(0);
	// }
	// // printf("setting socket flags\n");
	// // // set socket to nonblocking
	// // flags = fcntl(sock_fd, F_GETFL, NULL);
	// // if (flags < 0) {
	// // 	printf("flags get\n");
	// // 	assert(0);
	// // }
	// // printf("got socket flags\n");
	// // flags |= O_NONBLOCK;
	// // res = fcntl(sock_fd, F_SETFL, flags);
	// // if (res < 0) {
	// // 	printf("flags set\n");
	// // 	assert(0);
	// // }
	// // printf("set socket flags\n");

	// // connect() system call connects a socket to an address
	// printf("connecting\n");
	// res = connect(sock_fd, result->ai_addr, result->ai_addrlen);
	// printf("connect returned\n");
	// if (res != -1) {
	// 	printf("connected\n");
	// 	// break if we successfully established a connection
	// 	// break;
	// } else {
	// 	printf("unable to connect\n");
	// 	error = errno;
	// 	close(sock_fd);
	// 	printf("closed socket\n");
	// }
	// // }

	// freeaddrinfo(result);

	// // // rp is NULL only if we were not able to establish a connection
	// // if (rp == NULL) {
	// // 	printf("could not connect: %s\n", strerror(error));
	// // 	// return error;
	// // 	assert(0);
	// // }

	// printf("You are now connected to IP %s, port %s\n", server_ip, server_port);

	// // TODO: close the connection later when we won't use it anymore
	// close(sock_fd);



    return 0;
}