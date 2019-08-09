#!/bin/bash
set -e -v

# This script is for building GN on Windows.

UPLOAD_DIR=$PWD/public/build
COMPRESS_EXT=bz2

cd $GECKO_PATH

export PATH="$MOZ_FETCHES_DIR/ninja/bin:$PATH"
export PATH="$MOZ_FETCHES_DIR/mingw64/bin:$PATH"

. taskcluster/scripts/misc/vs-setup.sh
. taskcluster/scripts/misc/tooltool-download.sh
. taskcluster/scripts/misc/build-gn-common.sh

. $GECKO_PATH/taskcluster/scripts/misc/vs-cleanup.sh
