#!/bin/bash
set -x -e -v

# This scripts uses `cargo-vendor` to download all the dependencies needed
# to test `wgpu`, and exports those dependencies as a tarball.
# This avoids having to download these dependencies on every test job
# that tests `wgpu`.

UPLOAD_DIR=$HOME/artifacts

cd $GECKO_PATH
export PATH=$PATH:$MOZ_FETCHES_DIR/rustc/bin:$HOME/.cargo/bin
cargo install --version 0.1.23 cargo-vendor
cd gfx/wgpu/
mkdir .cargo
cargo-vendor vendor --relative-path --sync ./Cargo.toml > .cargo/config
mkdir wgpu-deps
mv vendor .cargo wgpu-deps/
mkdir wgpu-deps/cargo-apk
# Until there's a version of cargo-apk published on crates.io that has
# https://github.com/rust-windowing/android-rs-glue/pull/223, we need to use
# an unpublished version.
cargo install --path $MOZ_FETCHES_DIR/android-rs-glue/cargo-apk --root wgpu-deps/cargo-apk cargo-apk
tar caf wgpu-deps.tar.bz2 wgpu-deps

mkdir -p $UPLOAD_DIR
mv wgpu-deps.tar.bz2 $UPLOAD_DIR/
