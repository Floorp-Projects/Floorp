#!/bin/bash
set -e -v

# This script is for building GN on Linux.

WORKSPACE=$HOME/workspace
export CC=gcc
export CXX=g++
export LDFLAGS=-lrt

# Gn build scripts use #!/usr/bin/env python, which will be python 2.6 on
# the worker and cause failures. Work around this by putting python2.7
# ahead of it in PATH.
mkdir -p $WORKSPACE/python_bin
ln -s /usr/bin/python2.7 $WORKSPACE/python_bin/python
export PATH=$WORKSPACE/python_bin:$PATH

cd $GECKO_PATH

. taskcluster/scripts/misc/build-gn-common.sh
