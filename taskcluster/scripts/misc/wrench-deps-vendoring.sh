#!/bin/bash
set -x -e -v

# This scripts uses `cargo-vendor` to download all the dependencies needed
# to build `wrench` (a tool used for standalone testing of webrender), and
# exports those dependencies as a tarball. This avoids having to download
# these dependencies on every test job that uses `wrench`.

UPLOAD_DIR=$HOME/artifacts

cd $GECKO_PATH
. taskcluster/scripts/misc/tooltool-download.sh
export PATH=$PATH:$MOZ_FETCHES_DIR/rustc/bin
cargo install --version 0.1.23 cargo-vendor
cd gfx/wr/
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
cargo install --path $MOZ_FETCHES_DIR/android-rs-glue/cargo-apk --root wrench-deps/cargo-apk cargo-apk
tar caf wrench-deps.tar.bz2 wrench-deps

mkdir -p $UPLOAD_DIR
mv wrench-deps.tar.bz2 $UPLOAD_DIR/
