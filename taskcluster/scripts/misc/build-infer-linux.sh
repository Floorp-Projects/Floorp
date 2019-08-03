#!/bin/bash
set -x -e -v

# This script is for building infer for Linux.

cd $GECKO_PATH

# gets a bit too verbose here
set +x

cd build/build-infer
./build-infer.py -c infer-linux64.json

set -x

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp infer.tar.* $UPLOAD_DIR
