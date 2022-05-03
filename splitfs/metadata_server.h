#ifndef METADATA_SERVER_H
#define METADATA_SERVER_H

#include "remote.h"


// NOTE: this structure is recorded persistently
struct fserver_id {
    char ip_addr[64];
    char filepath[MAX_FILENAME_LEN];
};

// NOTE: this structure is recorded persistently
struct file_metadata {
    size_t length;
    struct fserver_id location;
};

struct pwrite_in {
    int client_fd;
    struct sockaddr_in* dst;
    char port[8];
};

#ifdef __cplusplus
extern "C" {
#endif 
struct sockaddr_in* choose_fileserver();
#ifdef __cplusplus
}
#endif


#endif