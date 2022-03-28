#!/bin/bash

PROGRAM_PATH=$1
SPLITFS_PATH=$HOME/SplitFS
PRELOAD_PATH=./splitfs/libnvp.so

cwd=$(pwd)
cd $SPLITFS_PATH

export LD_LIBRARY_PATH=./splitfs
export NVP_TREE_FILE=./splitfs/bin/nvp_nvp.tree

LD_PRELOAD=$PRELOAD_PATH $cwd/$PROGRAM_PATH

cd $cwd
