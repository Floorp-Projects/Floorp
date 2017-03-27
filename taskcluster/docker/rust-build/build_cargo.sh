#!/bin/bash -vex

set -x -e

: WORKSPACE ${WORKSPACE:=/home/worker}
: BRANCH ${BRANCH:=0.15.0}

set -v

# Configure and build cargo.

if test $(uname -s) = "Darwin"; then
  export MACOSX_DEPLOYMENT_TARGET=10.7
fi

# Build the initial cargo checkout, which can download a snapshot.
pushd ${WORKSPACE}/cargo
./configure --prefix=${WORKSPACE}/rustc --local-rust-root=${WORKSPACE}/rustc
make
make dist
make install
popd

# Build the version we want.
export PATH=$PATH:${WORKSPACE}/rustc/bin
pushd ${WORKSPACE}/cargo
make clean
git checkout ${BRANCH}
OPENSSL_DIR=/rustroot/cargo64 cargo install --root=${WORKSPACE}/rustc --force
popd
