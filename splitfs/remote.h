#ifndef REMOTE_H
#define REMOTE_H

#include <zookeeper/zookeeper.h>

#define CONFIG_PATH "../config"
#define BUFFER_SIZE 256

int cxn_fd;
pthread_t server_thread;

static zhandle_t *zh;

void server_thread_start(void *arg);
int read_from_socket(int sock, void* buf, size_t len);

// enum for request types. add to this to add new operations
// TODO: we should have separate operations for write/pwrite and read/pread
enum remote_request_type {
    PREAD = 0,
    PWRITE,
    CREATE,
    OPEN,
    CLOSE
};

#define MAX_FILENAME_LEN 256 // maximum filename size in ext4

// struct for sending a remote request. add more fields to this
// TODO: need a way to actually send written data? 
// TODO: don't include file path unless we absolutely have to
struct remote_request {
    enum remote_request_type type;
    int flags;
    int fd;
    size_t count;
    loff_t offset;
    mode_t mode;
    char file_path[MAX_FILENAME_LEN];
};

struct remote_response {
    enum remote_request_type type; // type of original request, just as a sanity check
    int fd;
    size_t return_value;
};

// options read from a config file
struct config_options {
    char metadata_server_port[8];
    char splitfs_server_port[8];
    char zookeeper_port[8];
    // allow up to 8 machines that may act as metadata servers,
    // splitfs servers, or zookeeper servers
    // these lists can overlap
    char metadata_server_ips[8][16];
    char splitfs_server_ips[8][16];
    char zookeeper_ips[8][16];

};

int parse_config(struct config_options *conf_opts, char* config_path);
void parse_comma_separated(char *ip_buffer, char ips[8][16]);


#endif // REMOTE_H