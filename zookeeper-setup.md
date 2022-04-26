# Zookeeper Instructions


## Building and installing Zookeeper
Run the following commands
```
sudo apt install maven libcppunit-dev default-jre default-jdk libconfig-dev

export JAVA_HOME=/usr/lib/jvm/java-11-openjdk-amd64/

cd SplitFS/zookeeper

mvn -DskipTests clean install

cd zookeeper-client/zookeeper-client-c

autoreconf -i

./configure

sudo make install

```

## Running Zookeeper
**TODO: instructions for replicated Zookeeper**

1. Compile SplitFS with the `CLIENT` or `SERVER` argument set to 1 on each node, depending on which role you would like that node to play. A node can only play one role. `SERVER` compiles SplitFS to run a file server at that node and `CLIENT` to run a client. The metadata server can be run on any machine regardless of compilation configuration. Setting both `CLIENT` and `SERVER` to 0 builds the local version of SplitFS only.
2. Edit the `config` file (currently lives in the SplitFS root) and possibly `zookeeper/conf/zoo.conf` on each node for your setup. Make sure that each instance uses the same configuration.
    - `zoo.conf` only needs to be edited if you want to change the port that Zookeeper runs on. 
    - `config` should be modified to use the IP addresses of your machines.
        - The `*_ips` configurations can take a comma-separated list (no spaces) of up to 8 IP addresses, although only the first is currently used.
        - The client reads the `splitfs_server_*` options to determine what server to connect to and the server uses `zookeeper_*` options to connect to the zookeeper instance. 
        - The client and server both read the `metadata_*` options to figure out how to connect to the metadata server.
        - Make sure that `zookeeper_port` matches the port specified in `zookeeper/conf/zoo.conf`. 
        - If you are using Chameleon Cloud, use the internal IP addresses (which probably start with 10) rather than public floating IPs (which probably start with 128 or 129).
3. On a file server node, `cd` to SplitFS/zookeeper and run `sudo bin/zkServer.sh start`. This starts the Zookeeper server running in the background with the configuration options specified in conf/zoo.cfg. 
4. On either a file server node or a dedicated machine, `cd` to SplitFS/splitfs and run `./metadata_server` to start the metadata server. 
5. On a file server node, cd to SplitFS/dist-tests and run `./setup.sh; ./run.sh ./server` to start a file server.
6. On a client node, cd to SplitFS/dist-tests and run `./setup.sh; ./run.sh ./client` to run the client program. 

If the client's IP addresses and port options are set correctly, the client should automatically connect 1) the Zookeeper server, 2) the metadata server, and 3) the file server. It will disconnect from both when the client finishes. Currently, clients and servers connect to Zookeeper and the metadata server, but don't use either service for anything - all system calls are handled solely via direct communication between the client and file server.

