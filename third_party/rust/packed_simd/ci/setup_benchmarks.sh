#!/usr/bin/env bash

set -ex

# Get latest ISPC binary for the target and put it in the path
git clone https://github.com/gnzlbg/ispc-binaries
cp ispc-binaries/ispc-${TARGET} ispc

# Rust-bindgen requires RUSTFMT
rustup component add rustfmt-preview
