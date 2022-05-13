## Metadata Server:

1. Run Zookeeper using `sudo zookeeper/bin/zkServer.sh start`.
2. `cd splitfs` and run `sh metadata.sh`. This compiles splitfs and sets the environment variables.
3. Run `g++ remote.o metadata_server.cpp -o metadata_server -L/usr/local/lib -lzookeeper_mt -DTHREADED -lpthread; ./metadata_server.sh;` to start the metadata server.

## File Server:

`cd dist_tests` and run `sh server_start.sh`. This compiles SplitFS in the `SERVER` mode and runs the `server.c` file.

## Client

`cd dist_tests` and run `sh client_start.sh`. This compiles SplitFS in the `CLIENT` mode and runs the `client.c` file. To run any other client program, just run `./setup.sh; ./run.sh ./client-program` after compiling SplitFS.
