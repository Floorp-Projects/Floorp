#!/bin/bash
set -x -e -v

# This script is for building clang for Linux.

WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build

cd $HOME_DIR/src

. taskcluster/scripts/misc/tooltool-download.sh

export PATH="$WORKSPACE/build/src/binutils/bin:$PATH"

# gets a bit too verbose here
set +x

cd build/build-clang
python3 ./build-clang.py -c clang-8-linux64-aarch64-cross.json

set -x

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp clang.tar.* $UPLOAD_DIR
