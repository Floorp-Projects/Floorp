#!/bin/bash
set -x -e -v

# This script is for building hfsplus for Linux.

cd $GECKO_PATH

. taskcluster/scripts/misc/tooltool-download.sh

export PATH=$PATH:$GECKO_PATH/clang/bin

build/unix/build-hfsplus/build-hfsplus.sh $MOZ_FETCHES_DIR

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp $MOZ_FETCHES_DIR/hfsplus-tools.tar.* $UPLOAD_DIR
