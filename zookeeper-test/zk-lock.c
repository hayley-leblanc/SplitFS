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

void* thread_acquire_lock(void *data)
{
    struct threadLock *lock_context;
    lock_context = (struct threadLock *)(data);
    zhandle_t *zh = lock_context->zh;
    char *lock_path = lock_context->lock_path;

    acquire_lock(zh, lock_path);
}

void* thread_release_lock(void *data)
{
    struct threadLock *lock_context;
    lock_context = (struct threadLock *)(data);
    zhandle_t *zh = lock_context->zh;
    char *lock_path = lock_context->lock_path;

    release_lock(zh, lock_path);
}

void* create_lock_context(zhandle_t *zh, char *lock_path)
{
    struct threadLock *lock_context = malloc(sizeof(struct threadLock));
    lock_context->zh = zh; 
    lock_context->lock_path = lock_path;
    return (void *)(lock_context) ;
}


int main(void) 
{

    static zhandle_t *zh;
    int ret, ret1, ret2;
    char buffer[512];
    char buffer1[1024];
    char buffer2[1024];

    pthread_t thread_ids[10];

    int thread_count = 0;

    zh = zookeeper_init("localhost:5555", watcher, 10000, 0, 0, 0);
    if (!zh) {
        perror("zookeeper_init");
        return errno;
    }

    ret1 = zoo_delete(zh, "/_locknode", -1);
    ret1 = zoo_delete(zh, "/_locknode/lock-1", -1);
    ret1 = zoo_delete(zh, "/_locknode/lock-2", -1);
    ret1 = zoo_delete(zh, "/_locknode/lock-3", -1);
    // return 0;

    ret1 = zoo_create(zh, "/_locknode", NULL, -1, &ZOO_OPEN_ACL_UNSAFE, 
        ZOO_PERSISTENT, buffer1, sizeof(buffer1));

    printf("Lock node parent: %s, %d\n", buffer1, ret1);

    int tmp;

    printf("\n\n**Enter a number to create node lock-1:\n\n");
    scanf("%d", &tmp);
    ret1 = zoo_create(zh, "/_locknode/lock-1", NULL, -1, &ZOO_OPEN_ACL_UNSAFE, 
        ZOO_EPHEMERAL, buffer1, sizeof(buffer1));

    printf("Lock node: %s, %d\n", buffer1, ret1);
    

    printf("\n\n**Enter a number to acquire lock-1:\n\n");
    scanf("%d", &tmp);

    pthread_create(&thread_ids[thread_count++], NULL, thread_acquire_lock, create_lock_context(zh, "/_locknode/lock-1"));

    // int x;
    // scanf("%d &x);


    printf("\n\n**Enter a number to create node lock-2:\n\n");
    scanf("%d", &tmp);

    ret2 = zoo_create(zh, "/_locknode/lock-2", NULL, -1, &ZOO_OPEN_ACL_UNSAFE, 
        ZOO_EPHEMERAL, buffer2, sizeof(buffer2));

    printf("New lock node: %s, %d\n", buffer2, ret2);


    printf("\n\n**Enter a number to acquire lock-2:\n\n");
    scanf("%d", &tmp);
    pthread_create(&thread_ids[thread_count++], NULL, thread_acquire_lock, create_lock_context(zh, "/_locknode/lock-2"));
    // acquire_lock(zh, "/_locknode/lock-2");

    printf("\n\n**Enter a number to release lock-1:\n\n");
    scanf("%d", &tmp);
    pthread_create(&thread_ids[thread_count++], NULL, thread_release_lock, create_lock_context(zh, "/_locknode/lock-1"));
    // release_lock(zh, "/_locknode/lock-1");


    printf("\n\n**Enter a number to create node lock-3:\n\n");
    scanf("%d", &tmp);
    ret2 = zoo_create(zh, "/_locknode/lock-3", NULL, -1, &ZOO_OPEN_ACL_UNSAFE, 
        ZOO_EPHEMERAL, buffer2, sizeof(buffer2));
    printf("New lock node: %s, %d\n", buffer2, ret2);


    printf("\n\n**Enter a number to acquire lock-3:\n\n");
    scanf("%d", &tmp);
    pthread_create(&thread_ids[thread_count++], NULL, thread_acquire_lock, create_lock_context(zh, "/_locknode/lock-3"));
    // acquire_lock(zh, "/_locknode/lock-3");
    
    printf("\n\n**Enter a number to release lock-2:\n\n");
    scanf("%d", &tmp);
    pthread_create(&thread_ids[thread_count++], NULL, thread_release_lock, create_lock_context(zh, "/_locknode/lock-2"));
    // release_lock(zh, "/_locknode/lock-2");

    printf("\n\n**Enter a number to release lock-3:\n\n");
    scanf("%d", &tmp);
    pthread_create(&thread_ids[thread_count++], NULL, thread_release_lock, create_lock_context(zh, "/_locknode/lock-3"));
    // release_lock(zh, "/_locknode/lock-3");

    for(int i=0; i<thread_count; i++)
        pthread_join(thread_ids[i], NULL);
    
    printf("All threads closed. Leaving.\n");
    zookeeper_close(zh);

    return 0;
}