#!/bin/bash
set -x -e -v

# This script is for building binutils for Linux.

WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
UPLOAD_DIR=$WORKSPACE/artifacts

cd $HOME_DIR/src

build/unix/build-binutils/build-binutils.sh $HOME_DIR

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp $HOME_DIR/binutils.tar.* $UPLOAD_DIR
