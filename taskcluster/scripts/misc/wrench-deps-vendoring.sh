#!/bin/bash
set -x -e -v

# This scripts uses `cargo-vendor` to download all the dependencies needed
# to build `wrench` (a tool used for standalone testing of webrender), and
# exports those dependencies as a tarball. This avoids having to download
# these dependencies on every test job that uses `wrench`.

WORKSPACE=$HOME/workspace
SRCDIR=$WORKSPACE/build/src
UPLOAD_DIR=$HOME/artifacts

cd $WORKSPACE
. $SRCDIR/taskcluster/scripts/misc/tooltool-download.sh
export PATH=$PATH:$SRCDIR/rustc/bin
cargo install --version 0.1.21 cargo-vendor
cd $SRCDIR/gfx/wr/
mkdir .cargo
cargo vendor --relative-path --sync ./Cargo.lock > .cargo/config
mkdir wrench-deps
mv vendor .cargo wrench-deps/
tar caf wrench-deps.tar.bz2 wrench-deps

mkdir -p $UPLOAD_DIR
mv wrench-deps.tar.bz2 $UPLOAD_DIR/
