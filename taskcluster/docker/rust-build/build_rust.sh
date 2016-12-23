#!/bin/bash -vex

set -x -e

: WORKSPACE ${WORKSPACE:=/home/worker}

CORES=$(nproc || grep -c ^processor /proc/cpuinfo || sysctl -n hw.ncpu)

set -v

# Configure and build rust.
OPTIONS="--enable-llvm-static-stdcpp --disable-docs"
OPTIONS+="--enable-debuginfo"
OPTIONS+="--release-channel=stable"
i586="i586-unknown-linux-gnu"
i686="i686-unknown-linux-gnu"
x64="x86_64-unknown-linux-gnu"
arm="arm-linux-androideabi"

mkdir -p ${WORKSPACE}/rust-build
pushd ${WORKSPACE}/rust-build
${WORKSPACE}/rust/configure --prefix=${WORKSPACE}/rustc \
  --target=${x64},${i586} ${OPTIONS}
make -j ${CORES}
make dist
make install
popd
