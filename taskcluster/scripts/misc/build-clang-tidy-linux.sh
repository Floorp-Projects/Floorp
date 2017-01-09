#!/bin/bash
set -x -e -v

# This script is for building clang for Linux.

WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
UPLOAD_DIR=$WORKSPACE/artifacts

cd $HOME_DIR/src

TOOLTOOL_MANIFEST=browser/config/tooltool-manifests/linux64/releng.manifest
. taskcluster/scripts/misc/tooltool-download.sh

# gets a bit too verbose here
set +x

cd build/build-clang
# |mach python| sets up a virtualenv for us!
../../mach python ./build-clang.py -c clang-tidy-linux64.json

set -x

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp clang-tidy.tar.* $UPLOAD_DIR
