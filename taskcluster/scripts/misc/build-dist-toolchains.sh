#!/bin/bash
set -x -e -v

# This script is for packaging toolchains suitable for use by distributed sccache.
WORKSPACE=$HOME/workspace
UPLOAD_DIR=$HOME/artifacts
TL_NAME="$1"

cd $WORKSPACE/build/src

. taskcluster/scripts/misc/tooltool-download.sh

sccache/sccache --package-toolchain $PWD/$TL_NAME/bin/$TL_NAME $HOME/artifacts/$TL_NAME-dist-toolchain.tar.xz
