#!/bin/bash
set -x -e -v

# This script is for building clang on a Linux host with android compiler rt
# libs.

cd $GECKO_PATH

. taskcluster/scripts/misc/tooltool-download.sh

# gets a bit too verbose here
set +x

cd build/build-clang
python3 ./build-clang.py -c clang-8-android.json

set -x

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp clang.tar.* $UPLOAD_DIR
