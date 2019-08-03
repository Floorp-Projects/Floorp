#!/bin/bash
set -x -e -v

# This script is for packaging toolchains suitable for use by distributed sccache.
TL_NAME="$1"

mkdir -p $HOME/artifacts

$MOZ_FETCHES_DIR/sccache/sccache --package-toolchain $MOZ_FETCHES_DIR/$TL_NAME/bin/$TL_NAME $HOME/artifacts/$TL_NAME-dist-toolchain.tar.xz
