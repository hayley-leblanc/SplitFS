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

#include "metadata_server.h"
#include "remote.h"

pthread_mutex_t server_fdset_lock;
fd_set server_fds;

// watcher callback function
void watcher(zhandle_t *zzh, int type, int state, const char *path,
             void *watcherCtx) {}

void cleanup();

int main() {
    int ret;
    struct config_options conf_opts;
    char addr_buf[64];
    pthread_attr_t thread_attr;
	pthread_t thread;

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

    ret = pthread_mutex_init(&server_fdset_lock, NULL);
    if (ret < 0) {
        perror("pthread_mutex_init");
    }

    // establish connections to servers
    ret = pthread_attr_init(&thread_attr);
    if (ret != 0) {
        perror("pthread_attr_init");
        cleanup();
        return ret;
    }

    ret = pthread_create(&thread, &thread_attr, splitfs_server_connect, &conf_opts);
    if (ret != 0) {
        perror("pthread create");
        cleanup();
        return ret;
    }

    ret = pthread_attr_destroy(&thread_attr);
	if (ret != 0) {
		perror("pthread_attr_destroy");
        cleanup();
		return ret;
	}

    
    cleanup();
    return 0;
}

void cleanup() {
    zookeeper_close(zh);
    pthread_mutex_destroy(&server_fdset_lock);
}

void* splitfs_server_connect(void* args) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    // TODO: global num_connections with lock around it?
    int i, num_connections = 0, num_ips = 0, opt = 1, listen_socket, ret, cxn_socket;
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

    ret = getaddrinfo(NULL, conf_opts->splitfs_server_port, &hints, &result);
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
        if (num_connections == num_ips) {
            sleep(5);
        } else {
            cxn_socket = accept(listen_socket, result->ai_addr, &result->ai_addrlen);
            printf("connected to a client with fd %d\n", cxn_socket);
            if (cxn_socket < 0) {
                perror("accept");
            }
            // TODO: handle locking errors
            pthread_mutex_lock(&server_fdset_lock);
            FD_SET(cxn_socket, &server_fds);
            num_connections++;
            pthread_mutex_unlock(&server_fdset_lock);
        }
    }

    return NULL;

}

