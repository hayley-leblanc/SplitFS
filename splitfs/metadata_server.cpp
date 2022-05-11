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
#include <arpa/inet.h>

#include "metadata_server.h"
#include "metadata_server.hpp"
#include "remote.h"

// #include "zk_lock.h"

using namespace std;

struct lockStructure
{
    char *lock_path;
    pthread_mutex_t *sync_lock;
};


void release_lock(zhandle_t *zh, char *lock_path);

void acquire_lock(zhandle_t *zh, char *lock_path);

int can_acquire_lock(zhandle_t *zh, char *lock_path, pthread_mutex_t *sync_lock);

void exists_watcher(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx);

// stat completion function
void stat_completion(int rc, const struct Stat *stat, 
                     const void *data) { }

void void_completion(int rc, const void *data) { }

void strings_completion(int rc, const struct String_vector *strings, const void *data) { }

// watcher callback function
void watcher(zhandle_t *zzh, int type, int state, const char *path,
             void *watcherCtx) {}

int main() {
    int ret;
    struct config_options conf_opts;
    char addr_buf[64];
    pthread_attr_t server_cxn_thread_attr, 
                    client_cxn_thread_attr, 
                    client_select_thread_attr,
                    server_select_thread_attr;
	
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
    
    // int ret;
    char buffer[512];    
    ret = zoo_create(zh, "/_locknode", NULL, -1, &ZOO_OPEN_ACL_UNSAFE, ZOO_PERSISTENT, buffer, sizeof(buffer));
    if(ret!=ZOK)
        printf("Error when creating root locknode %d\n", ret);
    else
        printf("\n\n\nCREATED ROOT LOCKNODE\n\n\n");
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

    // set up thread to listen for incoming server requests
    ret = pthread_attr_init(&server_select_thread_attr);
    if (ret != 0) {
        perror("pthread_attr_init");
        cleanup();
        return ret;
    }
    ret = pthread_create(&server_select_thread, &server_select_thread_attr, server_listen, &conf_opts);
    if (ret != 0) {
        perror("pthread_create");
        cleanup();
        return ret;
    }
    ret = pthread_attr_destroy(&server_select_thread_attr);
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
    struct sockaddr sockname;
    struct sockaddr_in *sockname_in;
    socklen_t socklen = sizeof(struct sockaddr);
    char addr_buf[INET_ADDRSTRLEN];
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
            ret = getpeername(cxn_socket, &sockname, &socklen);
            if (ret < 0) {
                perror("getsockname");
            } else {
                sockname_in = (struct sockaddr_in*)&sockname;
                inet_ntop(ip_protocol, &(sockname_in->sin_addr), addr_buf, INET_ADDRSTRLEN);
                printf("got connection to server with fd %d at IP %s\n", cxn_socket, addr_buf);
                pthread_mutex_lock(&server_fdset_lock);
                FD_SET(cxn_socket, &server_fds);
                fd_to_server_ip[cxn_socket] = *sockname_in;
                server_fd_vec.push_back(cxn_socket);
                printf("servers connected: %d\n", server_fd_vec.size());
                pthread_mutex_unlock(&server_fdset_lock);
                printf("done handling new connection\n");
            }
        }
    }

    return NULL;
}

void* server_listen(void* args) {
    struct timeval tv;
    int ret, nfds = 0;
    fd_set server_fds_copy;
    struct remote_request request;

    while(1) {
        tv.tv_sec = 0;
        tv.tv_usec = 100;

        pthread_mutex_lock(&server_fdset_lock);
        FD_ZERO(&server_fds_copy);
        for (int i = 0; i < server_fd_vec.size(); i++) {
            if (server_fd_vec[i] > nfds) {
                nfds = server_fd_vec[i];
            }
        }
        for (int i = 0; i <= nfds; i++) {
            if (FD_ISSET(i, &server_fds)) {
                FD_SET(i, &server_fds_copy);
            }
        }
        pthread_mutex_unlock(&server_fdset_lock);
        ret = select(nfds+1, &server_fds_copy, NULL, NULL, &tv);
        if (ret < 0) {
            perror("select");
        } else {
            // determine which fd is ready
            pthread_mutex_lock(&server_fdset_lock);
            for (int i = 0; i < server_fd_vec.size(); i++) {
                if (FD_ISSET(server_fd_vec[i], &server_fds_copy)) {
                    // TODO: right now, the only kind of message we can get 
                    // is that the server disconnected. update this if 
                    // we can get other messages
                    // assert that reading from the fd always fails to make sure 
                    // we remember to update this if anything needs to change here
                    int fd = server_fd_vec[i];
                    ret = read_from_socket(fd, &request, sizeof(struct remote_request));
                    assert(ret <= 0);
                    printf("server disconnected\n");
                    FD_CLR(server_fd_vec[i], &server_fds);
                    printf("closing server fd %d\n", server_fd_vec[i]);
                    close(server_fd_vec[i]);
                    fd_to_server_ip.erase(server_fd_vec[i]);
                    server_fd_vec.erase(server_fd_vec.begin()+i);
                    printf("servers connected: %d\n", server_fd_vec.size());
                    break;
                }
            }
            pthread_mutex_unlock(&server_fdset_lock);
        }
    }
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
    struct config_options *conf_opts = (struct config_options*)args;
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
                    ret = read_from_client(client_fd_vec[i], conf_opts);
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

int read_from_client(int client_fd, struct config_options *conf_opts) {
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
        printf("closing client fd %d\n", client_fd);
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
        case PWRITE:
            printf("client wants to write to a file\n");
            ret = manage_pwrite(client_fd, conf_opts, request, response);
            if (ret < 0) {
                return ret;
            }
            break;
        case PREAD:
            printf("client wants to read from a file\n");
            ret = manage_pread(client_fd, conf_opts, request, response);
            if (ret < 0) {
                return ret;
            }
            break;
    }

    if (request->type != PREAD) {
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
    }

    return 0;
}

// until data is written to a file, all of its metadata is stored here 
// on the metadata server, so the file is just created locally 
// TODO: LOCKING!!
int manage_create(int client_fd, struct remote_request *request, struct remote_response &response) {
	printf("manage_create - 1 \n");
    int fd = open(request->file_path, request->flags, request->mode);
	printf("manage_create - 2 \n");
    printf("created file on metadata server with fd %d\n", fd);
    strcpy(fd_to_name[fd], request->file_path);
    fd_to_client[fd] = client_fd;

    response.type = CREATE;
    response.fd = fd;
    response.return_value = fd;
	printf("manage_create - 3 \n");

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
    printf("closing file descriptor %d\n", request->fd);
    int ret = close(request->fd);
    printf("closed file\n");
    fd_to_name.erase(request->fd);
    fd_to_client.erase(request->fd);

    response.type = CLOSE;
    response.fd = request->fd;
    response.return_value = ret;

    printf("done closing file\n");

    return ret;
}

// TODO: locking
int manage_pwrite(int client_fd, struct config_options *conf_opts, struct remote_request *request, struct remote_response &response) {
	printf("manage_pwrite - 1 \n");
    struct sockaddr_in* sa;
    struct pwrite_in input;
    struct remote_request fileserver_notif;
    int fileserver_fd, ret;
    printf("manage_pwrite - 2 \n");
    printf("client wants to write %d bytes to offset %d\n", request->count, request->offset);
    
    acquire_lock(zh, fd_to_name[request->fd]);
	printf("manage_pwrite - 3 \n");

    // TODO: what if the file already lives somewhere? do a lookup
    sa = choose_fileserver(&fileserver_fd);
    input.client_fd = client_fd;
    input.dst = *sa;
    input.fileserver_fd = fileserver_fd;
    memcpy(input.port, conf_opts->splitfs_server_port, 8);
    strcpy(input.filepath, fd_to_name[request->fd]);
	printf("manage_pwrite - 4 \n");

    // tell fileservers to expect the file
    fileserver_notif.type = METADATA_WRITE_NOTIF;
    fileserver_notif.fd = request->fd;
    strcpy(fileserver_notif.file_path, fd_to_name[request->fd]);
    fileserver_notif.count = request->count;
    fileserver_notif.offset = request->offset;
	printf("manage_pwrite - 5 \n");

    ret = write(fileserver_fd, &fileserver_notif, sizeof(struct remote_request));
    if (ret < sizeof(remote_request)) {
        perror("write");
        return ret;
    }
	printf("manage_pwrite - 6 \n");
    printf("sent message to fileserver at fd %d\n", fileserver_fd);

    // since we don't yet have the buffer and have some extra info we need to pass 
    // into pwrite, use the buffer argument to store that info
    ret = pwrite(request->fd, &input, request->count, request->offset);
    printf("wrote %d bytes\n", ret);
	printf("manage_pwrite - 7 \n");

    release_lock(zh, fd_to_name[request->fd]);

    response.type = PWRITE;
    response.fd = request->fd;
    response.return_value = ret;
	printf("manage_pwrite - 8 \n");
    return 0;
}

int manage_pread(int client_fd, struct config_options *conf_opts, struct remote_request *request, struct remote_response &response) {
	printf("manage_pread - 1 \n");
    int bytes_read, ret, fileserver_fd = -1;
    struct file_metadata fm;
    struct metadata_response mr;
    struct remote_request fileserver_notif;
    char addr_buf[INET_ADDRSTRLEN];
    printf("client wants to read %d bytes at offset %d\n", request->count, request->offset);

    // we don't need to call splitfs to handle this. just read the local metadata file to see where 
    // the file lives, if it has content anywhere
    // the client-provided fd is the same one we use to read the file
	printf("manage_pread - 2 \n");

    bytes_read = pread(request->fd, &fm, sizeof(fm), 0);
    if (bytes_read < sizeof(fm)) {
        perror("pread");
        return bytes_read;
    }
	printf("manage_pread - 3 \n");

    // send a message to the server telling it to open the file 
    fileserver_notif.type = METADATA_READ_NOTIF;
    fileserver_notif.fd = request->fd;
    strcpy(fileserver_notif.file_path, fd_to_name[request->fd]);
    // we need to acquire the socket fd for the file server we want to communicate with
    // but making a map from sockaddr_in -> fds is tricky. so let's just iterate over 
    // the fd -> sockaddr_in map.
    for (map<int, struct sockaddr_in>::iterator it = fd_to_server_ip.begin(); it != fd_to_server_ip.end(); it++) {
        if (it->second.sin_addr.s_addr == fm.location.ip_addr.sin_addr.s_addr) {
            fileserver_fd = it->first;
        }
    }
	printf("manage_pread - 4 \n");
    if (fileserver_fd < 0) {
        printf("File does not live on any file servers\n");
        return 0;
    }
	printf("manage_pread - 5 \n");

    printf("sending notification to file server at fd %d\n", fileserver_fd);
    ret = write(fileserver_fd, &fileserver_notif, sizeof(struct remote_request));
    if (ret < sizeof(fileserver_notif)) {
        perror("write");
        return ret;
    }
    printf("sent notification to fileserver\n");
	printf("manage_pread - 6 \n");


    // build a response to tell the client where the file lives
    // we actually use the request object for this because it's set up better 
    // to send locations
    mr.type = PREAD;
    mr.fd = request->fd;
    mr.sa = fd_to_server_ip[fileserver_fd];
    memcpy(mr.port, conf_opts->splitfs_server_port, 8);

    inet_ntop(AF_INET, &(mr.sa.sin_addr), addr_buf, INET_ADDRSTRLEN);
	printf("%s\n", addr_buf);
	printf("manage_pread - 7 \n");

    ret = write(client_fd, &mr, sizeof(mr));
    if (ret < sizeof(mr)) {
        perror("write");
        return ret;
    }
	printf("manage_pread - 8 \n");
    return 0;
}

struct sockaddr_in* choose_fileserver(int *fd) {
    struct sockaddr_in *sa;
    // choose a file server to put (part of) a file on
    // for now just choose the first connected server
    printf("servers connected: %d\n", server_fd_vec.size());
    if (server_fd_vec.size() == 0) {
        printf("No file servers are connected\n");
        return NULL;
    }
    *fd = server_fd_vec[0];
    sa = &(fd_to_server_ip[*fd]);
    printf("sa: %p\n", sa);
    return sa;
}

void create_parent_nodes(char* lock_path)
{
    int ret;
    char buffer[500];
    for(int i=1; i<strlen(lock_path); i++)
    {
        if(lock_path[i]=='/')
        {
            // printf("%d\n", i);
            char tmp[500];
            strncpy(tmp, lock_path, i);
            tmp[i]='\0';
            ret = zoo_create(zh, tmp, NULL, -1, &ZOO_OPEN_ACL_UNSAFE, ZOO_PERSISTENT, buffer, sizeof(buffer));
            printf("Creating path %s, return code %d\n", tmp, ret);
        }
    }
}

// watcher function that processes when a node is cleared
void exists_watcher(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx)
{   
    if (type == ZOO_DELETED_EVENT) 
    {
        printf("Watcher: Node %s has been deleted\n", path);
        struct lockStructure *watcher_context;
        watcher_context = (struct lockStructure *)(watcherCtx);

        if(can_acquire_lock(zh, watcher_context->lock_path, watcher_context->sync_lock))
            acquire_lock(zh, watcher_context->lock_path);
    }
}

int can_acquire_lock(zhandle_t *zh, char *lock_path_, pthread_mutex_t *sync_lock)
{
    struct String_vector strs;

    char *root_lock_path = "/_locknode";

    char lock_path[100];
    lock_path[0] = '\0';
    strcat(lock_path, root_lock_path);
    strcat(lock_path, lock_path_);


    int chil = zoo_get_children(zh, root_lock_path, 0, &strs);

    printf("Trying to see if lock for %s can be acquired.\n", lock_path);

    for(int i=0; i<strs.count; i++)
    {
        char child_path[100];
        child_path[0] = '\0';
        strcat(child_path, "/_locknode/");
        strcat(child_path, strs.data[i]);

        printf("Comparing child_path %s with lock_path %s: %d\n", child_path, lock_path, strcmp(child_path, lock_path));
        if(strcmp(child_path, lock_path) < 0)
        {
            struct lockStructure watcher_context;
            watcher_context.lock_path = lock_path;
            watcher_context.sync_lock = sync_lock;

            void *watcherCtx = (void *)(&watcher_context); 

            printf("Setting watch on: %s", child_path);
            int ret = zoo_wexists(zh, child_path, exists_watcher, watcherCtx, NULL);
            return 0;
        }
    }
    pthread_mutex_unlock(sync_lock);
    printf("Can acquire lock for %s\n", lock_path);

    return 1;
}

void acquire_lock(zhandle_t *zh, char *lock_path_)
{
    printf("%s wants to acquire lock.\n", lock_path_);
    
    pthread_mutex_t sync_lock;
    pthread_mutex_init(&sync_lock, NULL);

    pthread_mutex_lock(&sync_lock);

    char *root_lock_path = "/_locknode";

    char lock_path[100];
    lock_path[0] = '\0';
    strcat(lock_path, root_lock_path);
    strcat(lock_path, lock_path_);

    int ret;
    char buffer[512];

    if(can_acquire_lock(zh, lock_path_, &sync_lock) && pthread_mutex_trylock(&sync_lock)==0)
    {
        create_parent_nodes(lock_path);
        ret = zoo_create(zh, lock_path, NULL, -1, &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL, buffer, sizeof(buffer));
        if(ret!=ZOK)
            printf("Error when creating locknode for %s: %d\n", lock_path, ret);

        printf("in loop Acquired lock for %s\n", lock_path);
        pthread_mutex_destroy(&sync_lock);
        return;
    }
    else
    {
        while(pthread_mutex_trylock(&sync_lock)!=0);
        create_parent_nodes(lock_path);
        ret = zoo_create(zh, lock_path, NULL, -1, &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL, buffer, sizeof(buffer));
        if(ret!=ZOK)
            printf("Error when creating locknode for %s: %d\n", lock_path, ret);

        pthread_mutex_destroy(&sync_lock);
        printf("outside loop Acquired lock for %s\n", lock_path);
        return;
    }
}

void release_lock(zhandle_t *zh, char *lock_path_)
{      
    char *root_lock_path = "/_locknode";

    char lock_path[100];
    lock_path[0] = '\0';
    strcat(lock_path, root_lock_path);
    strcat(lock_path, lock_path_);

    printf("%s releasing lock.\n", lock_path);
    int ret = zoo_delete(zh, lock_path, -1);
    return;
}

