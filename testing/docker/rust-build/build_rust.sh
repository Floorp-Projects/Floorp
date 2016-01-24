#!/bin/bash -vex

set -x -e

: WORKSPACE ${WORKSPACE:=/home/worker}

CORES=$(nproc || grep -c ^processor /proc/cpuinfo || sysctl -n hw.ncpu)

set -v

# Configure and build rust.
OPTIONS="--enable-rpath --enable-llvm-static-stdcpp --disable-docs"
x32="i686-unknown-linux-gnu"
x64="x86_64-unknown-linux-gnu"
arm="arm-linux-androideabi"

mkdir -p ${WORKSPACE}/rust-build
pushd ${WORKSPACE}/rust-build
${WORKSPACE}/rust/configure --prefix=${WORKSPACE}/rustc \
  --target=${x64},${x32} ${OPTIONS}
make -j ${CORES}
make install
popd

# Package the toolchain for upload.
pushd ${WORKSPACE}
tar cvJf rustc.tar.xz rustc/*
/build/tooltool.py add --visibility=public rustc.tar.xz
popd
