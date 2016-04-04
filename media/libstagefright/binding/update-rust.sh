#!/bin/sh
# Script to update mp4parse-rust sources to latest upstream

# Default version.
VER=v0.2.1

# Accept version or commit from the command line.
if test -n "$1"; then
  VER=$1
fi

echo "Fetching sources..."
rm -rf _upstream
git clone https://github.com/mozilla/mp4parse-rust _upstream/mp4parse
pushd _upstream/mp4parse
git checkout ${VER}
popd
rm -rf mp4parse
mkdir mp4parse
cp _upstream/mp4parse/src/*.rs mp4parse/
cp _upstream/mp4parse/include/mp4parse.h include/

# TODO: download deps from crates.io.

git clone https://github.com/BurntSushi/byteorder _upstream/byteorder
pushd _upstream/byteorder
git checkout 0.4.2
popd
rm -rf mp4parse/byteorder
mkdir mp4parse/byteorder
cp _upstream/byteorder/src/lib.rs mp4parse/byteorder/mod.rs
cp _upstream/byteorder/src/new.rs mp4parse/byteorder/new.rs

echo "Applying patches..."
patch -p4 < byteorder-mod.patch
patch -p4 < mp4parse-mod.patch
patch -p4 < mp4parse-thread.patch
patch -p4 < mp4parse-codecs.patch

echo "Cleaning up..."
rm -rf _upstream

echo "Updated to ${VER}."
