#!/bin/bash -vex

set -x -e

echo "running as" $(id)

set -v

cd $GECKO_PATH

export PATH=$PATH:$MOZ_FETCHES_DIR/node/bin

./mach browsertime --setup

# We have tools/browsertime/{package.json,node_modules,...} and want
# browsertime/{package.json,node_modules}.  ZIP because generic-worker
# doesn't support .tar.xz.
mkdir -p  /builds/worker/artifacts
cd tools
zip -r /builds/worker/artifacts/browsertime.zip browsertime
