#!/bin/bash -vex

set -x -e

: WORKSPACE ${WORKSPACE:=/home/worker}

set -v

# Configure and build cargo.

if test $(uname -s) = "Darwin"; then
  export MACOSX_DEPLOYMENT_TARGET=10.7
fi

pushd ${WORKSPACE}/cargo
./configure --prefix=${WORKSPACE}/rustc --local-rust-root=${WORKSPACE}/rustc
make
make dist
make install
popd
