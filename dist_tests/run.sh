#!/bin/bash

PROGRAM_PATH=$1
SPLITFS_PATH=$HOME/SplitFS
SPLITFS_PRELOAD_PATH=./splitfs/libnvp.so
ZOOKEEPER_PRELOAD_PATH=/usr/local/lib/libzookeeper_mt.so.2

cwd=$(pwd)
cd $SPLITFS_PATH

export LD_LIBRARY_PATH=./splitfs
export NVP_TREE_FILE=./splitfs/bin/nvp_nvp.tree

LD_PRELOAD="$SPLITFS_PRELOAD_PATH $ZOOKEEPER_PRELOAD_PATH" $cwd/$PROGRAM_PATH

cd $cwd
