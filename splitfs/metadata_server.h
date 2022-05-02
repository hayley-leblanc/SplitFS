#ifndef METADATA_SERVER_H
#define METADATA_SERVER_H

#include "remote.h"

// NOTE: these structures are recorded persistently in files
// and should not contain pointers to other objects

struct fserver_id {
    char ip_addr[64];
    char filepath[MAX_FILENAME_LEN];
};

struct file_metadata {
    size_t length;
    struct fserver_id location;
};

#endif