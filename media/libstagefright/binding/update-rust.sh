#!/bin/sh
# Script to update mp4parse-rust sources to latest upstream

# Default version.
VER=v0.4.0

# Accept version or commit from the command line.
if test -n "$1"; then
  VER=$1
fi

echo "Fetching sources..."
rm -rf _upstream
git clone https://github.com/mozilla/mp4parse-rust _upstream/mp4parse
pushd _upstream/mp4parse
git checkout ${VER}
echo "Constructing C api header..."
cargo build
popd
rm -rf mp4parse
mkdir -p mp4parse/src
cp _upstream/mp4parse/Cargo.toml mp4parse/
cp _upstream/mp4parse/build.rs mp4parse/
cp _upstream/mp4parse/src/*.rs mp4parse/src/
cp _upstream/mp4parse/include/mp4parse.h include/

# TODO: download deps from crates.io.

git clone https://github.com/BurntSushi/byteorder _upstream/byteorder
pushd _upstream/byteorder
git checkout 0.5.3
popd
rm -rf mp4parse/src/byteorder
mkdir mp4parse/src/byteorder
cp _upstream/byteorder/src/lib.rs mp4parse/src/byteorder/mod.rs
cp _upstream/byteorder/src/new.rs mp4parse/src/byteorder/new.rs

echo "Applying patches..."
patch -p4 < byteorder-mod.patch
patch -p4 < mp4parse-mod.patch
patch -p4 < mp4parse-cargo.patch

echo "Cleaning up..."
rm -rf _upstream

echo "Updated to ${VER}."
