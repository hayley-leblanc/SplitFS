#ifndef METADATA_SERVER_H
#define METADATA_SERVER_H

#include "remote.h"


// NOTE: this structure is recorded persistently
struct fserver_id {
    struct sockaddr_in ip_addr;
    char filepath[MAX_FILENAME_LEN];
};

struct Erasure {
    int flag;
    char fileA[256];
    char fileB[256];
    char fileP[256];
    char fileQ[256];
    
};

// NOTE: this structure is recorded persistently
struct file_metadata {
    size_t length;
    struct fserver_id location;
    struct Erasure erasure_coding;
};

struct pwrite_in {
    int client_fd;
    struct sockaddr_in dst;
    int fileserver_fd;
    char port[8];
    char filepath[MAX_FILENAME_LEN];
};

struct sockaddr_in* choose_fileserver(int *fd);



#endif