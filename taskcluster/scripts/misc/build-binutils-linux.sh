#!/bin/bash
set -x -e -v

# This script is for building binutils for Linux.

cd $GECKO_PATH

build/unix/build-binutils/build-binutils.sh $MOZ_FETCHES_DIR

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp $MOZ_FETCHES_DIR/binutils.tar.* $UPLOAD_DIR
