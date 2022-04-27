gcc zk-lock.c -o lock1 -L/usr/local/lib -lzookeeper_mt -DTHREADED -lpthread
./lock1
