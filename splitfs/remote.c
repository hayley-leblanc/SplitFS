#include "nv_common.h"
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>

#include "remote.h"
#include "fileops_nvp.h"

void server_listen(int sock_fd);
int remote_create(struct remote_request *request);

void server_thread_start(void *arg) {
    int accept_socket, sock_fd, res;
    struct addrinfo hints;
	struct addrinfo *result, *rp;
	char* server_port = "4444";
    int opt = 1;

    int ip_protocol = AF_INET; // IPv4
	int transport_protocol = SOCK_STREAM; // TCP

	DEBUG("opening connections to client\n");
	
	// set up a socket to listen for a connection request
	// at some point, this will have to run in the background
	// so that the server can do other work while also listening
	// for new requests
	sock_fd = socket(ip_protocol, transport_protocol, 0);
	if (sock_fd < 0) {
		DEBUG("socket failed: %s\n", strerror(errno));
		assert(0);
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = 0;
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	res = getaddrinfo(NULL, server_port, &hints, &result);
	if (res < 0) {
		DEBUG("getaddrinfo\n");
		assert(0);
	}
	
	sock_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (sock_fd < 0) {
		DEBUG("socket\n");
		assert(0);
	}

	// allow socket to be reused to avoid problems with binding in the future
	res = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (res < 0) {
		DEBUG("setsockopt failed: %s\n", strerror(errno));
		// return res;
		assert(0);
	}

	// bind the socket to the local address and port so we can accept connections on it
	// res = bind(sock_fd, (struct sockaddr*)&my_addr, addrlen);
	res = bind(sock_fd, result->ai_addr, result->ai_addrlen);
	if (res < 0) {
		DEBUG("bind failed: %s\n", strerror(errno));
		// return ret;
		assert(0);
	}

	// set up socket to listen for connections
	res = listen(sock_fd, 2);
	if (res < 0) {
		// perror("listen");
		DEBUG("listen failed: %s\n", strerror(errno));
		// return ret;
		assert(0);
	}

	// wait for someone to connect and accept when they do
	DEBUG("waiting for connections\n");
	accept_socket = accept(sock_fd, result->ai_addr, &result->ai_addrlen);
	if (accept_socket < 0) {
		DEBUG("accept failed: %s\n", strerror(errno));
		// return accept_socket;
		assert(0);
	}

	freeaddrinfo(result);

	// TODO: close these later when we won't need them anymore
	_hub_find_fileop("posix")->CLOSE(sock_fd); // I think we can close this one here?
	// _hub_find_fileop("posix")->CLOSE(accept_socket); // I think we want to save this one and close it later?
	cxn_fd = accept_socket;

    server_listen(cxn_fd);
}

// waits in a loop for messages from the client process.
// TODO: if the client disconnects, we should free connection resources and then
// wait for a new client to connect
// TODO: in the future if we are monitoring multiple file descriptors, use select()
void server_listen(int sock_fd) {
    int request_size = sizeof(struct remote_request);
    struct remote_request *request;
    char request_buffer[request_size];
    int bytes_read;
    int cur_index = 0;
        
    while(1) {
        // DEBUG("waiting for message from client\n");
		bytes_read = read_from_socket(sock_fd, request_buffer, request_size);
		if (bytes_read < request_size) {
			DEBUG("read failed, client is disconnected\n");
			_hub_find_fileop("posix")->CLOSE(sock_fd); 
			assert(0); // TODO: better error handling
		}
		request = (struct remote_request*)request_buffer;
		switch(request->type) {
			case CREATE:
				remote_create(request);
				break;
		}
        // bytes_read = read(sock_fd, request_buffer, request_size);
        // if (bytes_read < 0) {
        //     DEBUG("read failed, client is disconnected\n");
        //     _hub_find_fileop("posix")->CLOSE(sock_fd); 
        //     assert(0); // TODO: better error handling
        // } else if (bytes_read > 0) {
        //     cur_index += bytes_read;
        //     // read until we have the full request from the client
        //     while (cur_index < request_size) {
        //         bytes_read = read(sock_fd, request_buffer+cur_index, request_size-cur_index);
        //         if (bytes_read < 0) {
        //             DEBUG("read failed\n");
        //             _hub_find_fileop("posix")->CLOSE(sock_fd); 
        //             assert(0);
        //         }
        //         cur_index += bytes_read;
        //     }
        //     DEBUG("got message of %d bytes from the client!\n", cur_index);

        //     // perform an action based on the type of request made by the client
        //     request = (struct remote_request*)request_buffer;
        //     switch(request->type) {
        //         case CREATE:
        //             remote_create(request);
        //             break;
        //     }
        // }
    }
}

int remote_create(struct remote_request *request) {
    int fd, ret;
    struct remote_response response;

    fd = _nvp_OPEN(request->file_path, request->flags, request->mode);
    response.type = CREATE;
    response.fd = fd;
    ret = write(cxn_fd, &response, sizeof(response));
    if (ret < 0) {
        DEBUG("failed or partial write\n");
        return ret;
    }
    return 0;
}

int read_from_socket(int sock, void *buf, size_t len) {
	int bytes_read = read(sock, buf, len);
	if (bytes_read < 0) {
		DEBUG("read failed");
		return bytes_read;
	}
	int cur_index = bytes_read;
	while (cur_index < len) {
		bytes_read = read(sock, buf+cur_index, len-cur_index);
		if (bytes_read < 0) {
			DEBUG("read failed");
			return bytes_read;
		}
		cur_index += bytes_read;
	}
	return bytes_read;
}


