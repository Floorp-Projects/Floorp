#!/bin/bash
set -x -e -v

# This scripts uses `cargo-vendor` to download all the dependencies needed
# to build `wrench` (a tool used for standalone testing of webrender), and
# exports those dependencies as a tarball. This avoids having to download
# these dependencies on every test job that uses `wrench`.

UPLOAD_DIR=$HOME/artifacts

cd $GECKO_PATH
export PATH=$PATH:$MOZ_FETCHES_DIR/rustc/bin:$HOME/.cargo/bin
cd gfx/wr/
mkdir .cargo
cargo vendor --locked --sync ./Cargo.toml > .cargo/config
mkdir wrench-deps
mv vendor .cargo wrench-deps/

ci-scripts/install-meson.sh
mv meson wrench-deps/meson

mkdir -p $UPLOAD_DIR
tar caf $UPLOAD_DIR/wrench-deps.tar.zst wrench-deps
