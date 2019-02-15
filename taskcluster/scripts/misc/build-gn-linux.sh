#!/bin/bash
set -e -v

# This script is for building GN on Linux.

WORKSPACE=$HOME/workspace
UPLOAD_DIR=$HOME/artifacts
COMPRESS_EXT=xz
export CC=$WORKSPACE/build/src/gcc/bin/gcc
export CXX=$WORKSPACE/build/src/gcc/bin/g++

# Gn build scripts use #!/usr/bin/env python, which will be python 2.6 on
# the worker and cause failures. Work around this by putting python2.7
# ahead of it in PATH.
mkdir -p $WORKSPACE/python_bin
ln -s /usr/bin/python2.7 $WORKSPACE/python_bin/python
export PATH=$WORKSPACE/python_bin:$PATH

cd $WORKSPACE/build/src

. taskcluster/scripts/misc/tooltool-download.sh
. taskcluster/scripts/misc/build-gn-common.sh
