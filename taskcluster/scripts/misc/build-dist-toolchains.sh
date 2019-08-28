#!/bin/bash
set -x -e -v

# This script is for packaging toolchains suitable for use by distributed sccache.
TL_NAME="$1"

mkdir -p $HOME/artifacts
mkdir -p $HOME/toolchains

mv $MOZ_FETCHES_DIR/$TL_NAME $HOME/toolchains/$TL_NAME

$MOZ_FETCHES_DIR/sccache/sccache --package-toolchain $HOME/toolchains/$TL_NAME/bin/$TL_NAME $HOME/artifacts/$TL_NAME-dist-toolchain.tar.xz
