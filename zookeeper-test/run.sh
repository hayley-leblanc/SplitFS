#!/bin/bash

PROGRAM_PATH=$1

sudo LD_PRELOAD=/usr/local/lib/libzookeeper_mt.so.2 ./$PROGRAM_PATH