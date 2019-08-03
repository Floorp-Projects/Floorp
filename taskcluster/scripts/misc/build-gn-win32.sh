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

# Building with MSVC spawns a mspdbsrv process that keeps a dll open in the MSVC directory.
# This prevents the taskcluster worker from unmounting cleanly, and fails the build.
# So we kill it.
taskkill -f -im mspdbsrv.exe || true
