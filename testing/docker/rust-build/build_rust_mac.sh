#!/bin/bash -vex

set -e

: WORKSPACE ${WORKSPACE:=$PWD}
: TOOLTOOL ${TOOLTOOL:=python $WORKSPACE/tooltool.py}

CORES=$(nproc || grep -c ^processor /proc/cpuinfo || sysctl -n hw.ncpu)
echo Building on $CORES cpus...

OPTIONS="--enable-debuginfo --disable-docs"
TARGETS="x86_64-apple-darwin,i686-apple-darwin"

PREFIX=${WORKSPACE}/rustc

set -v

mkdir -p ${WORKSPACE}/gecko-rust-mac
pushd ${WORKSPACE}/gecko-rust-mac

export MACOSX_DEPLOYMENT_TARGET=10.7
${WORKSPACE}/rust/configure --prefix=${PREFIX} --target=${TARGETS} ${OPTIONS}
make -j ${CORES}

rm -rf ${PREFIX}
mkdir ${PREFIX}
make dist
make install
popd

# Package the toolchain for upload.
pushd ${WORKSPACE}
rustc/bin/rustc --version
tar cvjf rustc.tar.bz2 rustc/*
${TOOLTOOL} add --visibility=public --unpack rustc.tar.bz2
popd
