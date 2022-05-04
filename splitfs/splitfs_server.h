#ifndef SPLITFS_SERVER_H
#define SPLITFS_SERVER_H

enum fd_node_type {
    CLIENT_FD = 0,
    METADATA_FD,
    FILE_FD,
};

// small linked list implementation for storing fds and stuff
// a map would be faster, but I don't want to implement it
struct ll_node {
    struct ll_node* next;
    int fd; // local fd
    int remote_fd;
    char file_path[MAX_FILENAME_LEN];
    enum fd_node_type type;
};

struct ll_node *file_fd_list_head = NULL;
struct ll_node *file_fd_list_tail = NULL;
struct ll_node *peer_fd_list_head = NULL;
struct ll_node *peer_fd_list_tail = NULL;

void new_peer_fd_node(int socket, enum fd_node_type type) {
    struct ll_node* new_node = malloc(sizeof(struct ll_node));
    if (new_node == NULL) {
        perror("malloc");
        assert(0);
    }

    new_node->next = NULL;
    new_node->fd = socket;
    memset(new_node->file_path, 0, MAX_FILENAME_LEN);
    new_node->type = type;

    if (peer_fd_list_head == NULL) {
        peer_fd_list_head = new_node;
        peer_fd_list_tail = new_node;
    } else {
        peer_fd_list_tail->next = new_node;
        peer_fd_list_tail = new_node;
    }
}

void new_file_fd_node(int local_fd, int remote_fd) {
    struct ll_node* new_node = malloc(sizeof(struct ll_node));
    if (new_node == NULL) {
        perror("malloc");
        assert(0);
    }

    new_node->next = NULL;
    new_node->fd = local_fd;
    new_node->remote_fd = remote_fd;
    new_node->type = FILE_FD;

    if (file_fd_list_head == NULL) {
        file_fd_list_head = new_node;
        file_fd_list_tail = new_node;
    } else {
        file_fd_list_tail->next = new_node;
        file_fd_list_tail = new_node;
    }
}

void delete_file_fd_node(int local_fd) {
    struct ll_node *cur = file_fd_list_head;
    struct ll_node *prev = NULL;
    while (cur != NULL) {
        if (cur->fd == local_fd) {
            if (prev == NULL) {
                file_fd_list_head = cur->next;
            } else {
                prev->next = cur->next;
            }
            free(cur);
            break;
        }
        cur = cur->next;
    }
}

void delete_peer_fd_node(int socket) {
    struct ll_node *cur = peer_fd_list_head;
    struct ll_node *prev = NULL;
    while (cur != NULL) {
        if (cur->fd == socket) {
            if (cur == peer_fd_list_tail) {
                peer_fd_list_tail = prev;
            }
            if (prev == NULL) {
                peer_fd_list_head = cur->next;
            } else {
                prev->next = cur->next;
            }
            free(cur);
            break;
        }
        prev = cur;
        cur = cur->next;
    }
}


#endif 