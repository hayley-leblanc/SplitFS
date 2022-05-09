#ifndef ZK_LOCK
#define ZK_LOCK

#include <string.h>
#include <errno.h>

#include <zookeeper/zookeeper.h>

struct lockStructure
{
    char *lock_path;
    pthread_mutex_t *sync_lock;
};

struct threadLock
{
    zhandle_t *zh;
    char *lock_path;
};

void release_lock(zhandle_t *zh, char *lock_path);

void acquire_lock(zhandle_t *zh, char *lock_path);

int can_acquire_lock(zhandle_t *zh, char *lock_path, pthread_mutex_t *sync_lock);

void exists_watcher(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx);

void stat_completion(int rc, const struct Stat *stat, const void *data);
 
#endif /* ZK_LOCK */