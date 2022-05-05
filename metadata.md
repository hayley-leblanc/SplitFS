# Metadata Server

This documentation describes the metadata server and how it interacts with the other components of the system.

## Supported system calls

`open`, `close`, `pwrite`, `pread`

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
- `struct file_metadata` in splitfs/metadata_server.h. This structure contains the data that is stored persistently at the metadata server to record the full metadata for each file. It should be extended to be able to hold more file server locations. Note that this structure and `struct fserver_id` should **never** contain pointers to other structures since they are stored persistently; all data that referred to by either of these structs must live in these structs.

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

## How it works

### Connecting to the system

For the metadata server: when the metadata server process is started using `splitfs/metadata_server.sh`, it automatically attempts to connect to a Zookeeper server (which may be local or remote) and starts several threads to listen for client and server connections. The metadata server should be started before any file servers or clients.

For clients: when a program is run with the SplitFS library compiled with the `CLIENT` option, the client automatically establishes a connection with the metadata server at an IP address specified in a local configuration file. Depending on what operations the client program performs, it may also open short-lived connections to file servers in order to send or obtain file data.

For file servers: when a program is run with the SplitFS library compiled with the `FILE_SERVER` option, the server will automatically 1) establish a connection to the metadata server specified in its local configuration file and 2) start a thread to listen for incoming connections from clients. 

### Creating a file

1. A client program calls the `open()` system call with the `O_CREAT` flag. This is intercepted by SplitFS, which calls `_nvp_OPEN()` to handle it.
2. On the client side, SplitFS constructs a request message with the arguments to the `open()` call and sends it to the metadata server.
3. The metadata server receives the request and calls `open()` locally with the arguments it was sent by the client. It also constructs a `file_metadata` struct and writes it to its new local version of the file. At this time, the `file_metadata` structure is empty because the file has a size of 0 and has no data on any file servers yet.
4. The metadata server sends a response message to the client with the file descriptor obtained from its local `open()` call. The client and metadata server will both use this file descriptor to access the file until the client closes it.

### Opening a file

The process of opening a file is almost exactly the same as creating a file; the only differences is that the metadata server opens its existing version of the file rather than creating a new one. However, note that due to how SplitFS handles file opening and creation, there are separate locations in the client-side code to handle these cases.

### Closing a file

The process of closing a file is very similar to opening one; when the client calls `close()`, the client-side SplitFS constructs a close request and sends it to the metadata server, which closes the file descriptor and sends a confirmation to the client.

### Writing to a file

1. A client program calls `pwrite()`. SplitFS intercepts the call and invokes `_nvp_PWRITE()`.
2. Client-side SplitFS constructs a write request to send to the metadata server that includes the file descriptor to write to, the amount of data to write, and the offset to write to. The request does not include the data to be written.
3. The metadata server receives the write request. It reads its local version of the file to determine whether the file is empty or not. If it is empty, it selects file server(s) to put the new data onto; if it is not, it looks up the location(s) of the file's current contents.
4. The metadata server constructs a notification to send to each file server that will be involved in the write. This notification is required because the client is using a file descriptor to perform the write, but the file server does not know which file that descriptor refers to. The notification provides the file server with that information and tells it that a client will (most likely) be sending a request to write data to that file very soon.
5. The metadata server constructs a response to send to the client that lists the locations at which the client should try to write data to. It sends this response to the client.
6. Using the response it receives from the metadata server, the client sends write requests to each file server involved in the write.
7. The file servers each send a response indicating how many bytes were written to the *metadata server*. This allows the metadata server to correctly update its own metadata about the file (namely the size of the file). 
8. The metadata server sends a response to the client indicating how many bytes were written.

### Reading from a file
1. A client program calls `pread()`. SplitFS intercepts the call and invokes `_nvp_PREAD()`. 
2. Client-side SplitFS constructs a read request to send to the metadata server that includes the file descriptor to read from, the amount of data to read, and the offset to read from.
3. The metadata server receives the read request and reads its local version of the file to determine where the file's contents currently live. If the file descriptor is not valid or the file is empty, it returns this information to the client and the operation ends. 
4. The metadata looks up the filepath associated with the given file descriptor and sends this information to the file servers so that they will be able to handle incoming reads from the client.
5. The metadata server sends a response to the client that lists the file servers the client should read from. 
6. Using the response it receives from the metadata server, the client sends read requests to each file server with data that it wants to read.
7. The file servers send read data back to the client, which puts it together into the user-specified buffer.