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
cargo install --version 0.1.23 cargo-vendor
cd $SRCDIR/gfx/wr/
mkdir .cargo
cargo vendor --relative-path --sync ./Cargo.lock > .cargo/config
mkdir wrench-deps
mv vendor .cargo wrench-deps/
mkdir wrench-deps/cargo-apk
# Until there's a version of cargo-apk published on crates.io that has
# https://github.com/tomaka/android-rs-glue/pull/205 and
# https://github.com/tomaka/android-rs-glue/pull/207 and
# https://github.com/tomaka/android-rs-glue/pull/171 (see also
# https://github.com/tomaka/android-rs-glue/issues/204), we need to use
# an unpublished version.
cargo install --git https://github.com/staktrace/android-rs-glue --rev 486491e81819c3346d364a93fc1f3c0206d3ece0 --root wrench-deps/cargo-apk cargo-apk
tar caf wrench-deps.tar.bz2 wrench-deps

mkdir -p $UPLOAD_DIR
mv wrench-deps.tar.bz2 $UPLOAD_DIR/
