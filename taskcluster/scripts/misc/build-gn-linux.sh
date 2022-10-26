#!/bin/bash
set -e -v

# This script is for building GN on Linux.

WORKSPACE=$HOME/workspace
export CC=gcc
export CXX=g++
export LDFLAGS=-lrt

cd $GECKO_PATH

. taskcluster/scripts/misc/build-gn-common.sh
