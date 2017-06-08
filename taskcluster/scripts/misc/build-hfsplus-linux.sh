#!/bin/bash
set -x -e -v

# This script is for building hfsplus for Linux.
WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
UPLOAD_DIR=$WORKSPACE/artifacts

cd $HOME_DIR/src

. taskcluster/scripts/misc/tooltool-download.sh

export PATH=$PATH:$HOME_DIR/src/clang/bin

build/unix/build-hfsplus/build-hfsplus.sh $HOME_DIR

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp $HOME_DIR/hfsplus-tools.tar.* $UPLOAD_DIR
