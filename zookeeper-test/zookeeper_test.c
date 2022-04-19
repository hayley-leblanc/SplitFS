#include <string.h>
#include <errno.h>

#include <zookeeper/zookeeper.h>

static zhandle_t *zh;

// watcher callback function
void watcher(zhandle_t *zzh, int type, int state, const char *path,
             void *watcherCtx) {}

int main(void) {
    int ret;
    char buffer[512];

    // args:
    // 1. host:port pair
    // 2. global watcher callback function
    // 3. recv timeout
    // 4. client id of a previously established session that this client wants to connect to. 0 if not reconnecting
    // 5. context of a "handback" object that will be associated with the handle. can be null. no clue what this is for
    // 6. flags
    zh = zookeeper_init("localhost:5555", watcher, 10000, 0, 0, 0);
    if (!zh) {
        perror("zookeeper_init");
        return errno;
    }

    struct ACL CREATE_ONLY_ACL[] = {{ZOO_PERM_CREATE, ZOO_AUTH_IDS}};
    struct ACL_vector CREATE_ONLY = {1, CREATE_ONLY_ACL};

    ret = zoo_create(zh, "/test", NULL, -1, &CREATE_ONLY, 
        ZOO_PERSISTENT, buffer, sizeof(buffer));
    if (ret < 0) {
        perror("zookeeper_create");
        zookeeper_close(zh);
        return ret;
    }

    zookeeper_close(zh);

    return 0;
}