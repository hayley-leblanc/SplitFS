# Zookeeper Setup Instructions

Run the following commands
```
sudo apt install maven libcppunit-dev default-jre default-jdk

export JAVA_HOME=/usr/lib/jvm/java-11-openjdk-amd64/

cd SplitFS/zookeeper

mvn -DskipTests clean install

cd zookeeper-client/zookeeper-client-c

autoreconf -i

./configure

sudo make install

```
