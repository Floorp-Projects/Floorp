#!/bin/bash
set -x -e -v

# This script is for building clang for Linux.

cd $GECKO_PATH

. taskcluster/scripts/misc/tooltool-download.sh

export PATH="$GECKO_PATH/binutils/bin:$PATH"

# gets a bit too verbose here
set +x

python3 build/build-clang/build-clang.py -c $1

set -x

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp clang.tar.* $UPLOAD_DIR
