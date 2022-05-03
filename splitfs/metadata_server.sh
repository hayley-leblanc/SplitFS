#!/bin/bash

ZOOKEEPER_PRELOAD_PATH=/usr/local/lib/libzookeeper_mt.so.2
SPLITFS_PRELOAD_PATH=./libnvp.so
METADATA_PRELOAD_PATH=./metadata.so

export LD_LIBRARY_PATH=.
export NVP_TREE_FILE=./bin/nvp_nvp.tree
../dist_tests/setup.sh
LD_PRELOAD="$METADATA_PRELOAD_PATH $SPLITFS_PRELOAD_PATH $ZOOKEEPER_PRELOAD_PATH" ./metadata_server