#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <zookeeper/zookeeper.h>
#include <sys/select.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <err.h>
#include <unistd.h>
#include <iostream>
#include <fcntl.h>

#include "metadata_server.h"
#include "remote.h"

using namespace std;

// watcher callback function
void watcher(zhandle_t *zzh, int type, int state, const char *path,
             void *watcherCtx) {}

int main() {
    int ret;
    struct config_options conf_opts;
    char addr_buf[64];
    pthread_attr_t server_cxn_thread_attr, client_cxn_thread_attr, client_select_thread_attr;
	
    ret = parse_config(&conf_opts, CONFIG_PATH, fclose, fopen);
    if (ret < 0) {
        return ret;
    }

    // establish connection to zookeeper server
    // TODO: handle more than one zookeeper IP?
    strcpy(addr_buf, conf_opts.zookeeper_ips[0]);
	strcat(addr_buf, ":");
	strcat(addr_buf, conf_opts.zookeeper_port);

    printf("connecting to zookeeper\n");
	zoo_deterministic_conn_order(1);
	zh = zookeeper_init(addr_buf, watcher, 10000, 0, 0, 0);
    if (!zh) {
        perror("zookeeper_init");
        return -1;
    }
    
    // set up mutexes
    ret = pthread_mutex_init(&server_fdset_lock, NULL);
    if (ret < 0) {
        perror("pthread_mutex_init");
        return ret;
    }
    ret = pthread_mutex_init(&client_fdset_lock, NULL);
    if (ret < 0) {
        perror("pthread_mutex_init");
        return ret;
    }

    // set up thread to listen for new server connections
    ret = pthread_attr_init(&server_cxn_thread_attr);
    if (ret != 0) {
        perror("pthread_attr_init");
        cleanup();
        return ret;
    }
    ret = pthread_create(&server_cxn_thread, &server_cxn_thread_attr, splitfs_server_connect, &conf_opts);
    if (ret != 0) {
        perror("pthread_create");
        cleanup();
        return ret;
    }
    ret = pthread_attr_destroy(&server_cxn_thread_attr);
	if (ret != 0) {
		perror("pthread_attr_destroy");
        cleanup();
		return ret;
	}

    // set up thread to listen for new client connections
    ret = pthread_attr_init(&client_cxn_thread_attr);
    if (ret != 0) {
        perror("pthread_attr_init");
        cleanup();
        return ret;
    }
    ret = pthread_create(&client_cxn_thread, &client_cxn_thread_attr, client_connect, &conf_opts);
    if (ret != 0) {
        perror("pthread_create");
        cleanup();
        return ret;
    }
    ret = pthread_attr_destroy(&client_cxn_thread_attr);
    if (ret != 0) {
		perror("pthread_attr_destroy");
        cleanup();
		return ret;
	}

    // set up thread to listen for incoming client requests
    ret = pthread_attr_init(&client_select_thread_attr);
    if (ret != 0) {
        perror("pthread_attr_init");
        cleanup();
        return ret;
    }
    ret = pthread_create(&client_select_thread, &client_select_thread_attr, client_listen, &conf_opts);
    if (ret != 0) {
        perror("pthread_create");
        cleanup();
        return ret;
    }
    ret = pthread_attr_destroy(&client_select_thread_attr);
    if (ret != 0) {
		perror("pthread_attr_destroy");
        cleanup();
		return ret;
	}
    
    cleanup();
    return 0;
}

void cleanup() {    
    pthread_join(server_cxn_thread, NULL);
    pthread_join(client_cxn_thread, NULL);
    pthread_join(client_select_thread, NULL);
    pthread_mutex_destroy(&server_fdset_lock);
    pthread_mutex_destroy(&client_fdset_lock);
    zookeeper_close(zh);
}

void* splitfs_server_connect(void* args) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    unsigned int num_ips = 0;
    int i, opt = 1, listen_socket, ret, cxn_socket;
    struct config_options *conf_opts = (struct config_options*)args;
    int ip_protocol = AF_INET;
    int transport_protocol = SOCK_STREAM;

    FD_ZERO(&server_fds);

    // establish connections with servers. this should run in a loop and attempt to 
    // reconnect to servers that are disconnected

    for (i = 0; i < 8; i++) {
        if (strcmp(conf_opts->splitfs_server_ips[i], "") != 0) {
            num_ips++;
        } else {
            break;
        }
    }

    memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = 0;
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

    ret = getaddrinfo(NULL, conf_opts->metadata_server_port, &hints, &result);
	if (ret < 0) {
		perror("getaddrinfo");
        return NULL;
	}

    listen_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (listen_socket < 0) {
		perror("socket");
        return NULL;
	}

    // allow socket to be reused to avoid problems with binding in the future
	ret = setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (ret < 0) {
		perror("setsockopt");
        return NULL;
	}

    // bind the socket to the local address and port so we can accept connections
    ret = bind(listen_socket, result->ai_addr, result->ai_addrlen);
    if (ret < 0) {
        perror("bind");
        return NULL;
    }
    ret = listen(listen_socket, 8);
    if (ret < 0) {
        perror("listen");
        return NULL;
    }
    
    while(1) {
        // if all connections are active, sleep for 5 seconds before trying again
        // TODO: should probably sleep for a shorter amount of time
        if (server_fd_vec.size() == num_ips) {
            sleep(5);
        } else {
            cxn_socket = accept(listen_socket, result->ai_addr, &result->ai_addrlen);
            if (cxn_socket < 0) {
                perror("accept");
            }
            printf("got connection to server with fd %d\n", cxn_socket);
            // TODO: handle locking errors
            pthread_mutex_lock(&server_fdset_lock);
            FD_SET(cxn_socket, &server_fds);
            // num_server_connections++;
            server_fd_vec.push_back(cxn_socket);
            pthread_mutex_unlock(&server_fdset_lock);
        }
    }

    return NULL;

}

void* client_connect(void* args) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int i, opt = 1, listen_socket, ret, cxn_socket;
    struct config_options *conf_opts = (struct config_options*)args;
    int ip_protocol = AF_INET;
    int transport_protocol = SOCK_STREAM;

    FD_ZERO(&server_fds);

    memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = 0;
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

    ret = getaddrinfo(NULL, conf_opts->metadata_client_port, &hints, &result);
	if (ret < 0) {
		perror("getaddrinfo");
        return NULL;
	}

    listen_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (listen_socket < 0) {
		perror("socket");
        return NULL;
	}

    // allow socket to be reused to avoid problems with binding in the future
	ret = setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (ret < 0) {
		perror("setsockopt");
        return NULL;
	}

    // bind the socket to the local address and port so we can accept connections
    ret = bind(listen_socket, result->ai_addr, result->ai_addrlen);
    if (ret < 0) {
        perror("bind");
        return NULL;
    }
    ret = listen(listen_socket, 8);
    if (ret < 0) {
        perror("listen");
        return NULL;
    }
    
    while(1) {
        // no limit on the number of clients that can connect, so we always just 
        // block on accepting new ones
        cxn_socket = accept(listen_socket, result->ai_addr, &result->ai_addrlen);
        if (cxn_socket < 0) {
            perror("accept");
        }
        printf("got connection to client with fd %d\n", cxn_socket);
        // TODO: handle locking errors
        pthread_mutex_lock(&client_fdset_lock);
        FD_SET(cxn_socket, &client_fds);
        client_fd_vec.push_back(cxn_socket);
        pthread_mutex_unlock(&client_fdset_lock);
    }

    return NULL;

}

// TODO: handle dropped clients
void* client_listen(void* args) {
    struct timeval tv;
    int ret, nfds = 0;
    fd_set client_fds_copy;
    // watch the set of client fds to see when one has input
    while (1) {
        tv.tv_sec = 0;
        tv.tv_usec = 100;
        
        pthread_mutex_lock(&client_fdset_lock);
        FD_ZERO(&client_fds_copy);
        for (int i = 0; i < client_fd_vec.size(); i++) {
            if (client_fd_vec[i] > nfds) {
                nfds = client_fd_vec[i];
            }
        }
        for (int i = 0; i <= nfds; i++) {
            if (FD_ISSET(i, &client_fds)) {
                FD_SET(i, &client_fds_copy);
            }
        }
        pthread_mutex_unlock(&client_fdset_lock);
        ret = select(nfds+1, &client_fds_copy, NULL, NULL, &tv);
        if (ret < 0) {
            perror("select");
        } else {
            // determine which fd is ready
            pthread_mutex_lock(&client_fdset_lock);
            bool found_fd = false;
            for (int i = 0; i < client_fd_vec.size(); i++) {
                if (FD_ISSET(client_fd_vec[i], &client_fds_copy)) {
                    // TODO: spin off a new thread to handle this FD
                    int fd = client_fd_vec[i];
                    found_fd = true;
                    pthread_mutex_unlock(&client_fdset_lock);
                    ret = read_from_client(client_fd_vec[i]);
                    if (ret < 0) {
                        return NULL;
                    }

                }
            }
            if (!found_fd) {
                pthread_mutex_unlock(&client_fdset_lock);
            }
            
        } 
    }

    return NULL;
}

int read_from_client(int client_fd) {
    int request_size = sizeof(struct remote_request);
    struct remote_request *request;
    char request_buffer[request_size];
    struct remote_response response;
    int bytes_read, ret;

    printf("waiting for message from client\n");
    bytes_read = read_from_socket(client_fd, request_buffer, request_size);
    if (bytes_read < request_size) {
        printf("client has disconnected\n");
        pthread_mutex_lock(&client_fdset_lock);
        FD_CLR(client_fd, &client_fds);
        // TODO: find a faster method; this is inefficient.
        // we can't just pass in the index we found the client fd at 
        // when we called read_from_client(); the fd's spot in the list
        // may have changed between then and now (?)
        for (int i = 0; i < client_fd_vec.size(); i++) {
            if (client_fd_vec[i] == client_fd) {
                client_fd_vec.erase(client_fd_vec.begin()+i);
            }
        }
        close(client_fd);
        pthread_mutex_unlock(&client_fdset_lock);
        return 0;
    }
    request = (struct remote_request*)request_buffer;
    memset(&response, 0, sizeof(response));
    switch(request->type) {
        case CREATE:
            printf("the client wants to create a file\n");
            ret = manage_create(client_fd, request, response);
            if (ret < 0) {
                return ret;
            }
            break;
        case OPEN:
            printf("client wants to open a file\n");
            ret = manage_open(client_fd, request, response);
            if (ret < 0) {
                return ret;
            }
            break;
        case CLOSE:
            printf("client wants to close a file\n");
            ret = manage_close(client_fd, request, response);
            if (ret < 0) {
                return ret;
            }
            break;

    }

    // TODO: if we fail to send a response, then we should clean up any open 
    // fds that the client might have had
    // zookeeper will take care of releasing its locks, we just have to clean up
    // old data about its open files
    printf("sending response\n");
    ret = write(client_fd, &response, sizeof(struct remote_response));
    if (ret < 0) {
        perror("write");
        return ret;
    }

    return 0;
}

// until data is written to a file, all of its metadata is stored here 
// on the metadata server, so the file is just created locally 
// TODO: LOCKING!!
int manage_create(int client_fd, struct remote_request *request, struct remote_response &response) {
    int fd = open(request->file_path, request->flags, request->mode);
    printf("created file on metadata server with fd %d\n", fd);
    strcpy(fd_to_name[fd], request->file_path);
    fd_to_client[fd] = client_fd;

    response.type = CREATE;
    response.fd = fd;
    response.return_value = fd;

    return fd;
}

// TODO: locking
int manage_open(int client_fd, struct remote_request *request, struct remote_response &response) {
    int fd = open(request->file_path, request->flags, request->mode);
    printf("opened file on metadata server with fd %d\n", fd);
    strcpy(fd_to_name[fd], request->file_path);
    fd_to_client[fd] = client_fd;

    response.type = OPEN;
    response.fd = fd;
    response.return_value = fd;

    return fd;
}


// TODO: locking
int manage_close(int client_fd, struct remote_request *request, struct remote_response &response) {
    int ret = close(request->fd);
    fd_to_name.erase(request->fd);
    fd_to_client.erase(request->fd);

    response.type = CLOSE;
    response.fd = request->fd;
    response.return_value = ret;

    return ret;
}