#!/bin/bash

ZOOKEEPER_PRELOAD_PATH=/usr/local/lib/libzookeeper_mt.so.2
SPLITFS_PRELOAD_PATH=libnvp.so

LD_PRELOAD="$SPLITFS_PRELOAD_PATH $ZOOKEEPER_PRELOAD_PATH" ./metadata_server