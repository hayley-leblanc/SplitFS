#include "nv_common.h"
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>

#include "remote.h"
#include "fileops_nvp.h"

void server_listen(int sock_fd, struct addrinfo *addr_info);
int remote_create(struct remote_request *request);
int remote_write(struct remote_request *request);
int remote_read(struct remote_request *request);
int remote_open(struct remote_request *request);
int remote_close(struct remote_request *request);
int set_up_listen(int sock_fd, struct addrinfo *addr_info);

void* server_thread_start(void *arg) {
    int accept_socket, sock_fd, ret, metadata_fd;
    struct addrinfo hints;
	struct addrinfo *result, *rp;
    int opt = 1;
	struct config_options conf_opts;
    int ip_protocol = AF_INET; // IPv4
	int transport_protocol = SOCK_STREAM; // TCP

	DEBUG("reading configuration\n");
	ret = parse_config(&conf_opts, "config", _hub_find_fileop("posix")->FCLOSE, _hub_find_fileop("posix")->FOPEN);
	if (ret < 0) {
		DEBUG("unable to read configuration options\n");
		assert(0);
	}

	DEBUG("connecting to metadata server\n");
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = ip_protocol;
	hints.ai_socktype = transport_protocol;
	hints.ai_protocol = 0;
	// TODO: multiple possible metadata server IPs?
	ret = getaddrinfo(conf_opts.metadata_server_ips[0], conf_opts.metadata_server_port, &hints, &result);
	if (ret < 0) {
		DEBUG("getaddrinfo on %s failed: %s\n", conf_opts.metadata_server_ips[0], strerror(errno));
		assert(0);
	}

	metadata_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (metadata_fd < 0) {
		DEBUG("socket failed: %s\n", strerror(errno));
		assert(0);
	}

	ret = connect(metadata_fd, result->ai_addr, result->ai_addrlen);
	if (ret != -1) {
		DEBUG("now connected to metadata server %s port %s\n", conf_opts.metadata_server_ips[0], conf_opts.metadata_server_port);
	} else {
		DEBUG("unable to connect to metadata server\n");
		_hub_find_fileop("posix")->CLOSE(sock_fd);
	}

	DEBUG("opening connections to client\n");
	
	// // set up a socket to listen for a connection request
	// // at some point, this will have to run in the background
	// // so that the server can do other work while also listening
	// // for new requests
	// sock_fd = socket(ip_protocol, transport_protocol, 0);
	// if (sock_fd < 0) {
	// 	DEBUG("socket failed: %s\n", strerror(errno));
	// 	assert(0);
	// }

	memset(&hints, 0, sizeof(hints));
	memset(&result, 0, sizeof(result));
	hints.ai_family = ip_protocol;
	hints.ai_socktype = transport_protocol;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = 0;
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	ret = getaddrinfo(NULL, conf_opts.splitfs_server_port, &hints, &result);
	if (ret < 0) {
		DEBUG("getaddrinfo\n");
		assert(0);
	}
	
	sock_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (sock_fd < 0) {
		DEBUG("socket\n");
		assert(0);
	}

	// allow socket to be reused to avoid problems with binding in the future
	ret = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (ret < 0) {
		DEBUG("setsockopt failed: %s\n", strerror(errno));
		// return res;
		assert(0);
	}

	// bind the socket to the local address and port so we can accept connections on it
	// ret = bind(sock_fd, (struct sockaddr*)&my_addr, addrlen);
	ret = bind(sock_fd, result->ai_addr, result->ai_addrlen);
	if (ret < 0) {
		DEBUG("bind failed: %s\n", strerror(errno));
		// return ret;
		assert(0);
	}

	ret = set_up_listen(sock_fd, result);
	if (ret < 0) {
		DEBUG("set up listen failed\n");
		assert(0);
	}

	// TODO: close these later when we won't need them anymore
	// _hub_find_fileop("posix")->CLOSE(sock_fd); // I think we can close this one here?
	// _hub_find_fileop("posix")->CLOSE(accept_socket); // I think we want to save this one and close it later?
	// cxn_fd = accept_socket;

    server_listen(sock_fd, result);
	
	// TODO: free addr info structs when we are all done and quitting the program
}

int set_up_listen(int sock_fd, struct addrinfo *addr_info) {
	struct timeval timeout;
	int ret;
	timeout.tv_sec = 3; // the actual timeout should probably be very high
						// to account for situations where the client is still running 
						// but not sending any messages
	timeout.tv_usec = 0;

	// set up socket to listen for connections
	ret = listen(sock_fd, 2);
	if (ret < 0) {
		DEBUG("listen failed: %s\n", strerror(errno));
		return ret;
	}

	// wait for someone to connect and accept when they do
	DEBUG("waiting for connections\n");
	cxn_fd = accept(sock_fd, addr_info->ai_addr, &addr_info->ai_addrlen);
	if (cxn_fd < 0) {
		DEBUG("accept failed: %s\n", strerror(errno));
		return cxn_fd;
	}

	ret = setsockopt(cxn_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval));
	if (ret < 0) {
		DEBUG("setsockopt failed\n");
		return ret;
	}

	return 0;
}

// waits in a loop for messages from the client process.
// TODO: if the client disconnects, we should free connection resources and then
// wait for a new client to connect
// TODO: in the future if we are monitoring multiple file descriptors, use select()
void server_listen(int sock_fd, struct addrinfo *addr_info) {
    int request_size = sizeof(struct remote_request);
    struct remote_request *request;
    char request_buffer[request_size];
    int bytes_read, ret;
    int cur_index = 0;
        
    while(1) {
        DEBUG("waiting for message from client\n");
		bytes_read = read_from_socket(cxn_fd, request_buffer, request_size);
		if (bytes_read < request_size) {
			DEBUG("read %d bytes\n", bytes_read);
			DEBUG("read failed, client is disconnected\n");
			_hub_find_fileop("posix")->CLOSE(cxn_fd); 
			// assert(0); // TODO: better error handling
			// TODO: if the client disconnects, go back and wait for another one to connect

			// wait for another client to connect 
			ret = set_up_listen(sock_fd, addr_info);
			if (ret < 0) {
				DEBUG("set up listen failed\n");
				assert(0);
			}
		}
		request = (struct remote_request*)request_buffer;
		switch(request->type) {
			case CREATE:
				DEBUG("serving create request\n");
				remote_create(request);
				DEBUG("done serving create\n");
				break;
			case PWRITE:
				DEBUG("serving write request\n");
				remote_write(request);
				DEBUG("done serving write\n");
				break;
			case PREAD:
				DEBUG("serving read request\n");
				remote_read(request);
				DEBUG("done serving read\n");
				break;
			case OPEN:
				DEBUG("serving open request\n");
				remote_open(request);
				DEBUG("done serving open\n");
				break;
			case CLOSE:
				DEBUG("serving close request\n");
				remote_close(request);
				DEBUG("done serving close\n");
				break;
		}
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

int remote_write(struct remote_request *request) {
	int ret;
	struct remote_response response;

	// the client will send us a buffer of data, we need to read that 
	// from the socket first

	// TODO: could we run into issues with this if they send a really large
	// amount of data?
	char* write_buf = malloc(request->count);
	if (write_buf == NULL) {
		DEBUG("malloc failed\n");
		// return -ENOMEM;
		ret = -ENOMEM;
		goto write_respond;
	}

	ret = read_from_socket(cxn_fd, write_buf, request->count);
	if (ret < 0) {
		// return ret;
		goto write_respond;
	}

	ret = _nvp_PWRITE(request->fd, write_buf, request->count, request->offset);
	
	// send back a response with the error code
write_respond:
	free(write_buf);
	response.type = PWRITE;
	response.fd = request->fd;
	response.return_value = ret;
	ret = write(cxn_fd, &response, sizeof(response));
	if (ret < 0) {
		DEBUG("error sending write response\n");
		return ret;
	}

	return 0;
}

int remote_read(struct remote_request *request) {
	char *read_buf;
	struct remote_response response;
	int ret, bytes_read;

	// allocate a buffer to read the data into
	read_buf = malloc(request->count);
	if (read_buf == NULL) {
		DEBUG("malloc failed\n");
		bytes_read = -ENOMEM;
		goto read_respond;
	}

	// read data into buffer
	bytes_read = _nvp_PREAD(request->fd, read_buf, request->count, request->offset);
	if (bytes_read < 0) {
		goto read_respond;
	}

read_respond:
	// construct a response indicating return value
	response.type = PREAD;
	response.fd = request->fd;
	response.return_value = bytes_read;
	ret = write(cxn_fd, &response, sizeof(response));
	if (ret < 0) {
		DEBUG("error sending read response\n");
		return ret;
	}

	if (bytes_read > 0) {
		// then send the read data
		ret = write(cxn_fd, read_buf, bytes_read);
		if (ret < 0) {
			DEBUG("error sending read data\n");
			return ret;
		}
	}
	DEBUG("sent %d bytes to client\n", ret);

	// free the buffer
	free(read_buf);
	return 0;
}

int remote_open(struct remote_request *request) {
	struct remote_response response;
	int ret;

	ret = _nvp_OPEN(request->file_path, request->flags, request->mode);
	if (ret < 0) {
		DEBUG("error opening file %d\n", request->file_path);
	}

	response.type = CLOSE;
	response.fd = ret;
	response.return_value = ret;
	ret = write(cxn_fd, &response, sizeof(response));
	if (ret < 0) {
		DEBUG("error sending open response\n");
		return ret;
	}

	return 0;
}

int remote_close(struct remote_request *request) {
	struct remote_response response;
	int ret;

	ret = _nvp_CLOSE(request->fd);
	if (ret < 0) {
		DEBUG("error closing fd %d\n", request->fd);
	}

	response.type = CLOSE;
	response.fd = request->fd;
	response.return_value = ret;
	DEBUG("sending %d bytes to the client\n", sizeof(struct remote_response));
	ret = write(cxn_fd, &response, sizeof(struct remote_response));
	if (ret < 0) {
		DEBUG("error sending close response\n");
		return ret;
	}

	return 0;
}

int read_from_socket(int sock, void *buf, size_t len) {
	int bytes_read = read(sock, buf, len);
	if (bytes_read < 0) {
		DEBUG("read failed\n");
		return bytes_read;
	}
	int cur_index = bytes_read;
	while (cur_index < len) {
		bytes_read = read(sock, buf+cur_index, len-cur_index);
		if (bytes_read <= 0) {
			DEBUG("read failed\n");
			return bytes_read;
		}
		cur_index += bytes_read;
	}
	return bytes_read;
}

