#!/bin/bash -vex

set -x -e

echo "running as" $(id)

set -v

cd $GECKO_PATH

# Download toolchain artifacts.
. taskcluster/scripts/misc/tooltool-download.sh

export PATH=$PATH:$MOZ_FETCHES_DIR/node/bin

# We don't install ImageMagick, so this will fail.  Continue.
./mach browsertime --setup || true

# We have tools/browsertime/{package.json,node_modules,...} and want
# browsertime/{package.json,node_modules}.  ZIP because generic-worker
# doesn't support .tar.xz.
mkdir -p  /builds/worker/artifacts
cd tools
zip -r /builds/worker/artifacts/browsertime.zip browsertime
