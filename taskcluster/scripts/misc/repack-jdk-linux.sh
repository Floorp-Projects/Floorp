#!/bin/bash
set -x -e -v

# This script is for fetching and repacking the OpenJDK (for
# Linux)

UPLOAD_DIR=$HOME/project/gecko/jdk
AVD_JSON_CONFIG="$1"

mkdir -p $HOME/artifacts $UPLOAD_DIR

# Populate /builds/worker/.mozbuild/jdk
cd $GECKO_PATH
./mach python python/mozboot/mozboot/android.py --jdk-only

tar cavf $UPLOAD_DIR/jdk-linux.tar.zst -C /builds/worker/.mozbuild jdk

ls -al $UPLOAD_DIR
