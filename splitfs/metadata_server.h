#ifndef METADATA_SERVER_H
#define METADATA_SERVER_H

#include <map>
#include <vector>
#include "remote.h"

pthread_mutex_t server_fdset_lock, client_fdset_lock;
fd_set server_fds, client_fds;
pthread_t server_cxn_thread, client_cxn_thread, client_select_thread;
std::vector<int> server_fd_vec, client_fd_vec;

// TODO: it might make more sense for these to live in zookeeper? that would 
// probably help us recover if the metadata server goes down and we have
// to switch to another one
std::map<int, char[MAX_FILENAME_LEN]> fd_to_name; // map open fd to corresponding file name
std::map<int, int> fd_to_client; // map open fd to fd of its client

void* splitfs_server_connect(void* args);
void* client_connect(void* args);
void* client_listen(void* args);
int read_from_client(int client_fd);
int manage_create(int client_fd, struct remote_request *request, struct remote_response &response);
int manage_close(int client_fd, struct remote_request *request, struct remote_response &response);
void cleanup();

#endif // METADATA_SERVER_H