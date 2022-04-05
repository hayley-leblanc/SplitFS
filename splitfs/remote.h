#ifndef REMOTE_H
#define REMOTE_H

int cxn_fd;
pthread_t server_thread;

void server_thread_start(void *arg);

// enum for request types. add to this to add new operations
enum remote_request_type {
    READ = 0,
    WRITE,
    CREATE,
};

#define MAX_FILENAME_LEN 256 // maximum filename size in ext4

// struct for sending a remote request. add more fields to this
// TODO: need a way to actually send written data? 
struct remote_request {
    enum remote_request_type type;
    int flags;
    mode_t mode;
    char file_path[MAX_FILENAME_LEN];
};

struct remote_response {
    enum remote_request_type type; // type of original request, just as a sanity check
    int fd;
};

// structures and methods for associating file paths with open file descriptors
// assume we aren't renaming any files, so file names won't change under us
// TODO: implement a basic hashmap; iterating over a linked list is too slow
struct fd_path_node {
    struct fd_path_node *next;
    int fd;
    char file_path[MAX_FILENAME_LEN];
};

static struct fd_path_node *fd_list_head = NULL;
static struct fd_path_node *fd_list_tail = NULL;

static int add_fd_path_node(int fd, char* file_path) {
    DEBUG("adding fd %d for path %s\n", fd, file_path);
    if (fd_list_head == NULL) {
        fd_list_head = malloc(sizeof(struct fd_path_node));
        if (fd_list_head == NULL) {
            DEBUG("bad malloc\n");
            return -1;
        }
        fd_list_head->next = NULL;
        fd_list_head->fd = fd;
        memcpy(fd_list_head->file_path, file_path, MAX_FILENAME_LEN);
        fd_list_tail = fd_list_head;
    }
    else {
        struct fd_path_node *new_node = malloc(sizeof(struct fd_path_node));
        if (new_node == NULL) {
            DEBUG("bad malloc\n");
            return -1;
        }
        new_node->next = NULL;
        new_node->fd = fd;
        memcpy(new_node->file_path, file_path, MAX_FILENAME_LEN);
        fd_list_tail->next = new_node;
        fd_list_tail = new_node;
    }

    return 0;
}


#endif // REMOTE_H