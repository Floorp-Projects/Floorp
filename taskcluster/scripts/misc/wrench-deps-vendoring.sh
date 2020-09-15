#!/bin/bash
set -x -e -v

# This scripts uses `cargo-vendor` to download all the dependencies needed
# to build `wrench` (a tool used for standalone testing of webrender), and
# exports those dependencies as a tarball. This avoids having to download
# these dependencies on every test job that uses `wrench`.

UPLOAD_DIR=$HOME/artifacts
MESON_VER=0.55.1

cd $GECKO_PATH
export PATH=$PATH:$MOZ_FETCHES_DIR/rustc/bin:$HOME/.cargo/bin
cd gfx/wr/
mkdir .cargo
cargo vendor --sync ./Cargo.toml > .cargo/config
mkdir wrench-deps
mv vendor .cargo wrench-deps/
mkdir wrench-deps/cargo-apk
# Until there's a version of cargo-apk published on crates.io that has
# https://github.com/rust-windowing/android-rs-glue/pull/223, we need to use
# an unpublished version.
cargo install --path $MOZ_FETCHES_DIR/android-rs-glue/cargo-apk --root wrench-deps/cargo-apk cargo-apk

curl -L https://github.com/mesonbuild/meson/releases/download/$MESON_VER/meson-${MESON_VER}.tar.gz -o meson.tar.gz
tar -xf meson.tar.gz
mv meson-${MESON_VER} wrench-deps/meson
pushd wrench-deps/meson
mkdir bin
ln -s ../meson.py bin/meson
popd

mkdir -p $UPLOAD_DIR
tar caf $UPLOAD_DIR/wrench-deps.tar.bz2 wrench-deps
