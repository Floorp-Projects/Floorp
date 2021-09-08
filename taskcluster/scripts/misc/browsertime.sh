#!/bin/bash -vex

set -x -e

echo "running as" $(id)

set -v

cd $GECKO_PATH

export PATH=$PATH:$MOZ_FETCHES_DIR/node/bin

./mach browsertime --setup

# We have tools/browsertime/{package.json,node_modules,...} and want
# browsertime/{package.json,node_modules}.
mkdir -p  /builds/worker/artifacts
cd tools
tar caf /builds/worker/artifacts/browsertime.tar.zst browsertime
