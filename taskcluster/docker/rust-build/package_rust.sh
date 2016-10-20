#!/bin/bash -vex

set -x -e

: WORKSPACE ${WORKSPACE:=/home/worker}

set -v

# Package the toolchain for upload.
pushd ${WORKSPACE}
tar cvJf rustc.tar.xz rustc/*
/build/tooltool.py add --visibility=public --unpack rustc.tar.xz
popd
