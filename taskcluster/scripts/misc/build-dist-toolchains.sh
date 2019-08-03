#!/bin/bash
set -x -e -v

# This script is for packaging toolchains suitable for use by distributed sccache.
TL_NAME="$1"

cd $GECKO_PATH

. taskcluster/scripts/misc/tooltool-download.sh

sccache/sccache --package-toolchain $GECKO_PATH/$TL_NAME/bin/$TL_NAME $HOME/artifacts/$TL_NAME-dist-toolchain.tar.xz
