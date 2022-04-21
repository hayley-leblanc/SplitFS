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

On a server, before starting SplitFS, `cd` to SplitFS/zookeeper and run `sudo bin/zkServer.sh start`. This starts the Zookeeper server running in the background with the configuration options specified in conf/zoo.cfg. Then, start the SplitFS server and clients. If the client's IP addresses and port options are set correctly, the client should automatically connect to both the Zookeeper server and the SplitFS server.

The files in SplitFS/dist-tests are set up properly to run with Zookeeper. `cd` to SplitFS/dist-tests and run `./setup.sh; gcc client.c -o client; ./run.sh ./client` to run a sample client program. Replace `client` with `server` to run the server. **NOTE**: SplitFS must be compiled with the correct `CLIENT` or `SERVER` option corresponding to whether you want that node to run the client or server version. If you try to run the server program with client SplitFS or vice versa, it will behave strangely.