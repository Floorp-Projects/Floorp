#!/bin/bash -vex

set -x -e

: WORKSPACE ${WORKSPACE:=/home/worker}

CORES=$(nproc || grep -c ^processor /proc/cpuinfo || sysctl -n hw.ncpu)

set -v

# Configure and build rust.
OPTIONS="--enable-rpath --enable-llvm-static-stdcpp --disable-docs"
M32="i686-unknown-linux-gnu"
M64="x86_64-unknown-linux-gnu"

mkdir -p ${WORKSPACE}/rust-build
pushd ${WORKSPACE}/rust-build
mkdir -p ${M32}
pushd ${M32}
${WORKSPACE}/rust/configure --prefix=${PWD}/../rustc --build=${M32} ${OPTIONS}
make -j ${CORES}
popd
mkdir -p ${M64}
pushd ${M64}
${WORKSPACE}/rust/configure --prefix=${PWD}/../rustc --build=${M64} ${OPTIONS}
make -j ${CORES}
make install
popd
# Copy 32 bit stdlib into the install directory for cross support.
cp -r ${M32}/${M32}/stage2/lib/rustlib/${M32} rustc/lib/rustlib/
popd

# Package the toolchain for upload.
pushd ${WORKSPACE}/rust-build
tar cvJf rustc.tar.xz rustc/*
/build/tooltool.py add --visibility=public rustc.tar.xz
popd
