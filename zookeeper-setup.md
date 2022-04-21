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

First, edit the `config` file (currently lives in the SplitFS root) and possibly `zookeeper/conf/zoo.conf` for your setup. `zoo.conf` only needs to be edited if you want to change the port that Zookeeper runs on. `config` should be modified to use the IP addresses of your machines. The `*_ips` configurations can take a comma-separated list (no spaces) of up to 8 IP addresses, although only the first is currently used. At the moment, `metadata_server_*` configuration options are not used. The client reads the `splitfs_server_*` options to determine what server to connect to and the server uses `zookeeper_*` options to connect to the zookeeper instance. Make sure that `zookeeper_port` matches the port specified in `zookeeper/conf/zoo.conf`. If you are using Chameleon Cloud, use the internal IP addresses (which probably start with 10) rather than public floating IPs (which probably start with 128 or 129). Make sure that each instance uses the same configurations.

On a server, before starting SplitFS, `cd` to SplitFS/zookeeper and run `sudo bin/zkServer.sh start`. This starts the Zookeeper server running in the background with the configuration options specified in conf/zoo.cfg. Then, start the SplitFS server, then run a client process. If the client's IP addresses and port options are set correctly, the client should automatically connect to both the Zookeeper server and the SplitFS server, and disconnect from both when the client program finishes.

The files in SplitFS/dist-tests are set up properly to run with Zookeeper. `cd` to SplitFS/dist-tests and run `./setup.sh; gcc client.c -o client; ./run.sh ./client` to run a sample client program. Replace `client` with `server` to run the server. 
