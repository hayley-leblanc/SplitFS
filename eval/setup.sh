#!/bin/bash

SPLITFS_PATH=$HOME/SplitFS

export LD_LIBRARY_PATH=$SPLITFS_PATH/splitfs
export NVP_TREE_FILE=$SPLITFS_PATH/splitfs/bin/nvp_nvp.tree

sudo umount /dev/pmem0 > /dev/null
sudo mkfs.ext4 -b 4096 /dev/pmem0
sudo mount -o dax /dev/pmem0 /mnt/pmem_emul
sudo chown -R $USER:$USER /mnt/pmem_emul