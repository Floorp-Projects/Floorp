#!/bin/bash
set -e

# This script is for building GCC for Linux.

WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
UPLOAD_DIR=$WORKSPACE/artifacts

cd $HOME_DIR/src

build/unix/build-gcc/build-gcc.sh $HOME_DIR

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp $HOME_DIR/gcc.tar.* $UPLOAD_DIR
