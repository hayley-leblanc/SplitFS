#include <string.h>
#include <errno.h>

#include <zookeeper/zookeeper.h>

#include <pthread.h>


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

// watcher callback function
void watcher(zhandle_t *zzh, int type, int state, const char *path,
             void *watcherCtx) {}

// stat completion function
void stat_completion(int rc, const struct Stat *stat, 
                     const void *data) { }

int can_acquire_lock(zhandle_t *zh, char *lock_path, pthread_mutex_t *sync_lock);

void acquire_lock(zhandle_t *zh, char *lock_path);

// watcher function that processes when a node is cleared
void exists_watcher(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx)
{   
    if (type == ZOO_DELETED_EVENT) 
    {
        printf("Watcher: Node %s has been deleted\n", path);
        struct lockStructure *watcher_context;
        watcher_context = (struct lockStructure *)(watcherCtx);

        if(can_acquire_lock(zh, watcher_context->lock_path, watcher_context->sync_lock))
            acquire_lock(zh, watcher_context->lock_path);
    }
}

int can_acquire_lock(zhandle_t *zh, char *lock_path, pthread_mutex_t *sync_lock)
{
    struct String_vector strs;

    char *root_lock_path = "/_locknode";
    int chil = zoo_get_children(zh, root_lock_path, 0, &strs);

    printf("Trying to see if lock for %s can be acquired.\n", lock_path);

    for(int i=0; i<strs.count; i++)
    {
        char child_path[100];
        child_path[0] = '\0';
        strcat(child_path, "/_locknode/");
        strcat(child_path, strs.data[i]);

        printf("Comparing child_path %s with lock_path %s: %d\n", child_path, lock_path, strcmp(child_path, lock_path));
        if(strcmp(child_path, lock_path) < 0)
        {
            struct lockStructure watcher_context;
            watcher_context.lock_path = lock_path;
            watcher_context.sync_lock = sync_lock;

            void *watcherCtx = (void *)(&watcher_context); 

            printf("Setting watch on: %s", child_path);
            int ret = zoo_wexists(zh, child_path, exists_watcher, watcherCtx, NULL);
            return 0;
        }
    }
    pthread_mutex_unlock(sync_lock);
    printf("Can acquire lock for %s\n", lock_path);

    return 1;
}

void acquire_lock(zhandle_t *zh, char *lock_path)
{
    printf("%s wants to acquire lock.\n", lock_path);
    
    pthread_mutex_t sync_lock;
    pthread_mutex_init(&sync_lock, NULL);

    pthread_mutex_lock(&sync_lock);

    if(can_acquire_lock(zh, lock_path, &sync_lock) && pthread_mutex_trylock(&sync_lock)==0)
    {
        printf("in loop Acquired lock for %s\n", lock_path);
        pthread_mutex_destroy(&sync_lock);
        return;
    }
    else
    {
        while(pthread_mutex_trylock(&sync_lock)!=0);
        pthread_mutex_destroy(&sync_lock);
        printf("outside loop Acquired lock for %s\n", lock_path);
        return;
    }
}

void release_lock(zhandle_t *zh, char *lock_path)
{   
    printf("%s releasing lock.\n", lock_path);
    int ret = zoo_delete(zh, lock_path, -1);
    return;
}
