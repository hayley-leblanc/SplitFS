# Metadata Server

This documentation describes the metadata server and how it interacts with the other components of the system.

## Files
- splitfs/metadata_server.cpp: Contains the core code that runs the metadata server. This program is responsible for managing the set of clients and servers currently connected to the system and orchestrating file operations. It uses a local instance of SplitFS to persistently store file metadata.
- splitfs/remote.c: Contains some helper functions for networking and reading configuration files.
- splitfs/splitfs_server.c: Contains functions that run on file servers to establish connections with clients and the metadata server and to manage reads/writes by clients.
- splitfs/fileops_nvp.c: Contains most of the key functionality of SplitFS. All client-side networking and processing is implemented here, as well as some metadata server stuff and a very small amount of file server stuff.
    - Key regions to look at: 
        - The beginning of `_nvp_do_pwrite()` contains some code that runs on the metadata server when a client sends a write request. 
        - The end of `_nvp_OPEN()` contains the code where the metadata server initializes metadata for a newly-created file.
        - `_nvp_OPEN()` also contains code that handles sending a request to open or create a file from a client to the metadata server.
        - `_nvp_CLOSE()` contains code that handles sending a request to close a file from a client to the metadata server.
        - `_nvp_PREAD()` contains code that handles sending a request to read a file from a client to the metadata server, and sending requests for the data from the client to file server(s).
        - `_nvp_PWRITE()` contains code that handles sending a request to write data to a file from a client to the metadata server, and sending requests to write data from the client to file server(s).
    - In general, grep/ctrl-F for `#if CLIENT` in splitfs/fileops_nvp.c to find code that runs on the client and `#if METADATA_SERVER` to find code that runs on the metadata server. A small amount of code is also included under `FILE_SERVER` directives, but most of the file server code lives in splitfs/splitfs_server.c

## Useful functions, variables, types
- `metadata_server_fd` in splitfs/fileops_nvp.c. When a client program starts, it establishes a connection with the metadata server. Subsequent network communication with the metadata server can be done using this global file descriptor.
- `read_from_socket` in splitfs/remote.c. Helps read incoming data from a network socket.
- `struct remote_request` in splitfs/remote.h. Main structure used to send requests for operations between nodes. Clients send this to the metadata server when they want to perform a system call, and the metadata server uses it to inform file servers of incoming client requests.
- `struct remote_response` in splitfs/remote.h. Smaller structure used to respond to requests (when a response is necessary).
- `struct metadata_response` in splitfs/remote.h. Sent from the metadata server to a client to tell the client the file server location of a requested file.

## Metadata server architecture
The metadata server runs four threads:

1. Server connection thread
2. Client connection thread
3. Server select thread
4. Client select thread

The connection threads constantly listen for incoming connections by clients or servers. The metadata server expects servers to connect using the `metadata_server_port` specified in the config file, and clients using the `metadata_client_port`. When a node (client or server) connects, the file descriptor for the socket representing the connection is saved in a vector and in an `fd_set` object. 

The select threads constantly poll the connections waiting for incoming network traffic. They use the `select` system call, which takes an `fd_set` specifying which file descriptors we want to listen on. When this system call indicates that a file descriptor has some new content to read, the thread reads the incoming message and handles it appropriately. At the moment, the metadata server can only handle one client and one server request at a time - it does not spin off additional threads for new requests.

## File server architecture
The file server runs two threads, a connection thread and a select thread. These threads serve a similar purpose as the client connection and select threads in the metadata server. 

When the file server starts up, it establishes a connection with the metadata server that it continues to use throughout execution. 