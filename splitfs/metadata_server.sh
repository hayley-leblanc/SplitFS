#!/bin/bash

ZOOKEEPER_PRELOAD_PATH=/usr/local/lib/libzookeeper_mt.so.2

LD_PRELOAD="$ZOOKEEPER_PRELOAD_PATH" ./metadata_server