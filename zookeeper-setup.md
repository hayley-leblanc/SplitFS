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

**NOTE**: SplitFS must be compiled with the correct `CLIENT` or `SERVER` option corresponding to whether you want that node to run the client or server version. If you try to run the server program with client SplitFS or vice versa, it will behave strangely. These options can be found in SplitFS/splitfs/common.mk. If you want to run a client, set `CLIENT=1` and `SERVER=0` (vice versa to run a server), then `make clean` and recompile.

On a server, before starting SplitFS, `cd` to SplitFS/zookeeper and run `sudo bin/zkServer.sh start`. This starts the Zookeeper server running in the background with the configuration options specified in conf/zoo.cfg. Then, start the SplitFS server, then run a client process. If the client's IP addresses and port options are set correctly, the client should automatically connect to both the Zookeeper server and the SplitFS server, and disconnect from both when the client program finishes.

The files in SplitFS/dist-tests are set up properly to run with Zookeeper. To use these tests, first, modify the `server_ip` variable in `_nvp_init2` in SplitFS/splitfs/fileops_nvp.c to the IP address of your server. If you are running on Chameleon Cloud, this should be the internal local IP (probably starts with 10) of the server, not the public IP (which probably starts with 128 or 129). Right now, Zookeeper must be run on the same node as the server instance of SplitFS. `cd` to SplitFS/dist-tests and run `./setup.sh; gcc client.c -o client; ./run.sh ./client` to run a sample client program. Replace `client` with `server` to run the server. 