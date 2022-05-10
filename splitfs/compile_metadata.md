g++ remote.o metadata_server.cpp -o metadata_server -L/usr/local/lib -lzookeeper_mt -DTHREADED; ./metadata_server.sh;
