#include "nv_common.h"
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>

#include "remote.h"
#include "fileops_nvp.h"
#include "splitfs_server.h"

void* server_listen(void* args);
void* server_connect(void* args);
int handle_metadata_notif(struct ll_node *node);
int handle_client_request(struct ll_node *node);
// void server_listen(int sock_fd, struct addrinfo *addr_info);
// int remote_create(struct remote_request *request);
// int remote_write(struct remote_request *request);
// int remote_read(struct remote_request *request);
// int remote_open(struct remote_request *request);
// int remote_close(struct remote_request *request);
// int set_up_listen(int sock_fd, struct addrinfo *addr_info);

pthread_t server_cxn_thread, server_select_thread;
pthread_mutex_t fdset_lock;
fd_set fdset;
int connected_peers = 0;
int metadata_server_fd;

void* server_thread_start(void *arg) {
    int accept_socket, sock_fd, ret;
    struct addrinfo hints;
	struct addrinfo *result, *rp;
    int opt = 1;
	struct config_options conf_opts;
    int ip_protocol = AF_INET; // IPv4
	int transport_protocol = SOCK_STREAM; // TCP
	pthread_attr_t server_cxn_thread_attr, server_select_thread_attr;

	FD_ZERO(&fdset);

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

	metadata_server_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (metadata_server_fd < 0) {
		DEBUG("socket failed: %s\n", strerror(errno));
		assert(0);
	}

	// allow socket to be reused to avoid problems with binding in the future
	ret = setsockopt(metadata_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (ret < 0) {
		DEBUG("setsockopt failed: %s\n", strerror(errno));
		// return res;
		assert(0);
	}

	ret = connect(metadata_server_fd, result->ai_addr, result->ai_addrlen);
	if (ret != -1) {
		DEBUG("now connected to metadata server %s port %s\n", conf_opts.metadata_server_ips[0], conf_opts.metadata_server_port);
	} else {
		perror("connect");
		DEBUG("unable to connect to metadata server\n");
		_hub_find_fileop("posix")->CLOSE(metadata_server_fd);
		assert(0);
	}

	pthread_mutex_lock(&fdset_lock);
	FD_SET(metadata_server_fd, &fdset);
	new_peer_fd_node(metadata_server_fd, METADATA_FD);
	connected_peers++;
	pthread_mutex_unlock(&fdset_lock);

	// set up threads to listen for client connections and to select on incoming cxns
	// TODO: clean up nicely if something goes wrong

	ret = pthread_mutex_init(&fdset_lock, NULL);
	if (ret < 0) {
		perror("pthread_mutex_init");
		return NULL;
	}

	ret = pthread_attr_init(&server_cxn_thread_attr);
	if (ret != 0) {
		perror("pthread_attr_init");
		return NULL;
	}
	ret = pthread_create(&server_cxn_thread, &server_cxn_thread_attr, server_connect, &conf_opts);
	if (ret != 0) {
        perror("pthread_create");
        return NULL;
    }
	ret = pthread_attr_destroy(&server_cxn_thread_attr);
	if (ret != 0) {
		perror("pthread_attr_destroy");
		return NULL;
	}

	ret = pthread_attr_init(&server_select_thread_attr);
	if (ret != 0) {
		perror("pthread_attr_init");
		return NULL;
	}
	ret = pthread_create(&server_select_thread, &server_select_thread_attr, server_listen, &conf_opts);
	if (ret != 0) {
        perror("pthread_create");
        return NULL;
    }
	ret = pthread_attr_destroy(&server_select_thread_attr);
	if (ret != 0) {
		perror("pthread_attr_destroy");
		return NULL;
	}

	pthread_join(server_select_thread, NULL);
	pthread_join(server_cxn_thread, NULL);
}

void* server_connect(void* args) {
	struct addrinfo hints;
    struct addrinfo *result;
	struct config_options *conf_opts = (struct config_options*)args;
    int ip_protocol = AF_INET;
    int transport_protocol = SOCK_STREAM;
	int sock_fd, ret, cxn_socket;
	int opt = 1;

	// set up a socket to listen for incoming client connections
	memset(&hints, 0, sizeof(hints));
	memset(&result, 0, sizeof(result));
	hints.ai_family = ip_protocol;
	hints.ai_socktype = transport_protocol;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = 0;
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	ret = getaddrinfo(NULL, conf_opts->splitfs_server_port, &hints, &result);
	if (ret < 0) {
		perror("getaddrinfo");
        return NULL;
	}

	sock_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (sock_fd < 0) {
		DEBUG("socket\n");
		return NULL;
	}

	// allow socket to be reused to avoid problems with binding in the future
	ret = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (ret < 0) {
		DEBUG("setsockopt failed: %s\n", strerror(errno));
		return NULL;
	}

	// bind the socket to the local address and port so we can accept connections on it
	// ret = bind(sock_fd, (struct sockaddr*)&my_addr, addrlen);
	ret = bind(sock_fd, result->ai_addr, result->ai_addrlen);
	if (ret < 0) {
		DEBUG("bind failed: %s\n", strerror(errno));
		// return ret;
		assert(0);
	}

	ret = listen(sock_fd, 8);
    if (ret < 0) {
        perror("listen");
        return NULL;
    }

	// listen for incoming client connections
	
	while(1) {
		cxn_socket = accept(sock_fd, result->ai_addr, &result->ai_addrlen);
		if (cxn_socket < 0) {
			perror("accept");
			assert(0);
		}
		pthread_mutex_lock(&fdset_lock);
		FD_SET(cxn_socket, &fdset);
		new_peer_fd_node(cxn_socket, CLIENT_FD);
		connected_peers++;
		pthread_mutex_unlock(&fdset_lock);
	}

	// TODO: clean stuff up if something fails
	return NULL;
}

void* server_listen(void* args) {
    struct timeval tv;
    int ret, nfds = 0;
    fd_set fdset_copy;
    struct remote_request request;
	struct ll_node* cur;

    while(1) {
        tv.tv_sec = 0;
        tv.tv_usec = 100;

        pthread_mutex_lock(&fdset_lock);
        FD_ZERO(&fdset_copy);
		cur = peer_fd_list_head;
		while (cur != NULL) {
			if (cur->fd > nfds) {
				nfds = cur->fd;
			}
			cur = cur->next;
		}
        for (int i = 0; i <= nfds; i++) {
            if (FD_ISSET(i, &fdset)) {
                FD_SET(i, &fdset_copy);
            }
        }
        pthread_mutex_unlock(&fdset_lock);
        ret = select(nfds+1, &fdset_copy, NULL, NULL, &tv);
        if (ret < 0) {
            perror("select");
        } else {
            // determine which fd is ready
            pthread_mutex_lock(&fdset_lock);
			cur = peer_fd_list_head;
			while (cur != NULL) {
				if (FD_ISSET(cur->fd, &fdset_copy)) {
					// fd can either be for a connection with a client or with 
					// the metadata server
					// these functions handle requests and also disconnection
					switch (cur->type) {
						case METADATA_FD:
							ret = handle_metadata_notif(cur);
							if (ret < 0) {
								DEBUG("failed reading metadata server notification");
							}
							break;
						case CLIENT_FD:
							ret = handle_client_request(cur);
							if (ret < 0) {
								DEBUG("failed handling client request\n");
							}
							break;
					}
					break;
				}
				cur = cur->next;
			}
			pthread_mutex_unlock(&fdset_lock);
        }
    }
	return NULL;
}

int handle_metadata_notif(struct ll_node *node) {
	int ret, bytes_read, local_file_fd;
	struct remote_request notif;
	
	bytes_read = read_from_socket(node->fd, &notif, sizeof(struct remote_request));
	if (bytes_read < sizeof(struct remote_request)) {
		return -1;
	}
	// TODO: use a switch statement if we end up using more notification types
	if (notif.type == METADATA_WRITE_NOTIF) {
		// 1. open or create the file
		local_file_fd = _nvp_OPEN(notif.file_path, O_CREAT | O_RDWR);
		if (local_file_fd < 0) {
			perror("open");
			return local_file_fd;
		}
		// 2. save the file descriptor
		new_file_fd_node(local_file_fd, notif.fd);
		DEBUG("added local fd %d to record for remote fd %d\n", local_file_fd, notif.fd);
	} else {
		DEBUG("unrecognized notification type\n");
		assert(0);
	}

	return 0;
}

int handle_client_request(struct ll_node *node) {
	int ret, bytes_read, remote_fd, local_fd = 0;
	struct remote_request request;
	struct remote_response response;
	struct ll_node *cur;
	char *data_buf;

	// receive request with metadata about write - offset, length, etc.
	bytes_read = read_from_socket(node->fd, &request, sizeof(struct remote_request));
	if (bytes_read < sizeof(struct remote_request)) {
		return -1;
	}

	// find the local fd to write to
	remote_fd = request.fd;
	cur = file_fd_list_head;
	while (cur != NULL) {
		if (cur->remote_fd == remote_fd) {
			local_fd = cur->fd;
			break;
		}
		cur = cur->next;
	}
	if (cur == NULL || local_fd == 0) {
		DEBUG("requested file is not open\n");
		return -1;
	}

	// allocate space to receive the incoming data
	data_buf = malloc(request.count);
	if (data_buf == NULL) {
		perror("malloc");
		return -1;
	}

	// receive data
	ret = read_from_socket(node->fd, data_buf, request.count);
	if (ret < 0) {
		return ret;
	}

	ret = _nvp_PWRITE(local_fd, data_buf, request.count, request.offset);

	// send the response with the number of bytes written to the METADATA SERVER
	// so it can update metadata/coalesce multiple responses from multiple file servers
	// before sending a single final response to the client
	response.type = PWRITE;
	response.fd = request.fd;
	response.return_value = ret;

	DEBUG("sending response to metadata server\n");
	ret = write(metadata_server_fd, &response, sizeof(struct remote_response));
	if (ret < 0) {
		DEBUG("failed writing response to metadata server\n");
	}
	DEBUG("sent response to metadata server\n");


	// TODO: keep the file open for longer in case we want to read/write to it again soon
	delete_file_fd_node(local_fd);
	close(local_fd);
	return 0;
}


// int remote_write(struct remote_request *request) {
// 	int ret;
// 	struct remote_response response;

// 	// the client will send us a buffer of data, we need to read that 
// 	// from the socket first

// 	// TODO: could we run into issues with this if they send a really large
// 	// amount of data?
// 	char* write_buf = malloc(request->count);
// 	if (write_buf == NULL) {
// 		DEBUG("malloc failed\n");
// 		// return -ENOMEM;
// 		ret = -ENOMEM;
// 		goto write_respond;
// 	}

// 	ret = read_from_socket(cxn_fd, write_buf, request->count);
// 	if (ret < 0) {
// 		// return ret;
// 		goto write_respond;
// 	}

// 	ret = _nvp_PWRITE(request->fd, write_buf, request->count, request->offset);
	
// 	// send back a response with the error code
// write_respond:
// 	free(write_buf);
// 	response.type = PWRITE;
// 	response.fd = request->fd;
// 	response.return_value = ret;
// 	ret = write(cxn_fd, &response, sizeof(response));
// 	if (ret < 0) {
// 		DEBUG("error sending write response\n");
// 		return ret;
// 	}

// 	return 0;
// }

// int remote_read(struct remote_request *request) {
// 	char *read_buf;
// 	struct remote_response response;
// 	int ret, bytes_read;

// 	// allocate a buffer to read the data into
// 	read_buf = malloc(request->count);
// 	if (read_buf == NULL) {
// 		DEBUG("malloc failed\n");
// 		bytes_read = -ENOMEM;
// 		goto read_respond;
// 	}

// 	// read data into buffer
// 	bytes_read = _nvp_PREAD(request->fd, read_buf, request->count, request->offset);
// 	if (bytes_read < 0) {
// 		goto read_respond;
// 	}

// read_respond:
// 	// construct a response indicating return value
// 	response.type = PREAD;
// 	response.fd = request->fd;
// 	response.return_value = bytes_read;
// 	ret = write(cxn_fd, &response, sizeof(response));
// 	if (ret < 0) {
// 		DEBUG("error sending read response\n");
// 		return ret;
// 	}

// 	if (bytes_read > 0) {
// 		// then send the read data
// 		ret = write(cxn_fd, read_buf, bytes_read);
// 		if (ret < 0) {
// 			DEBUG("error sending read data\n");
// 			return ret;
// 		}
// 	}
// 	DEBUG("sent %d bytes to client\n", ret);

// 	// free the buffer
// 	free(read_buf);
// 	return 0;
// }

// int remote_open(struct remote_request *request) {
// 	struct remote_response response;
// 	int ret;

// 	ret = _nvp_OPEN(request->file_path, request->flags, request->mode);
// 	if (ret < 0) {
// 		DEBUG("error opening file %d\n", request->file_path);
// 	}

// 	response.type = CLOSE;
// 	response.fd = ret;
// 	response.return_value = ret;
// 	ret = write(cxn_fd, &response, sizeof(response));
// 	if (ret < 0) {
// 		DEBUG("error sending open response\n");
// 		return ret;
// 	}

// 	return 0;
// }

// int remote_close(struct remote_request *request) {
// 	struct remote_response response;
// 	int ret;

// 	ret = _nvp_CLOSE(request->fd);
// 	if (ret < 0) {
// 		DEBUG("error closing fd %d\n", request->fd);
// 	}

// 	response.type = CLOSE;
// 	response.fd = request->fd;
// 	response.return_value = ret;
// 	DEBUG("sending %d bytes to the client\n", sizeof(struct remote_response));
// 	ret = write(cxn_fd, &response, sizeof(struct remote_response));
// 	if (ret < 0) {
// 		DEBUG("error sending close response\n");
// 		return ret;
// 	}

// 	return 0;
// }
