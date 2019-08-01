#!/bin/bash
set -e -v

# This script is for building GN on Windows.

WORKSPACE=$PWD
UPLOAD_DIR=$WORKSPACE/public/build
COMPRESS_EXT=bz2

export PATH="$WORKSPACE/build/src/ninja/bin:$PATH"
export PATH="$WORKSPACE/build/src/mingw64/bin:$PATH"

cd $WORKSPACE/build/src

. taskcluster/scripts/misc/vs-setup.sh
. taskcluster/scripts/misc/tooltool-download.sh
. taskcluster/scripts/misc/build-gn-common.sh

# Building with MSVC spawns a mspdbsrv process that keeps a dll open in the MSVC directory.
# This prevents the taskcluster worker from unmounting cleanly, and fails the build.
# So we kill it.
taskkill -f -im mspdbsrv.exe || true
