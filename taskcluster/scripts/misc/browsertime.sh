#!/bin/bash -vex

set -x -e

echo "running as" $(id)

: WORKSPACE ${WORKSPACE:=/builds/worker/workspace}

set -v

cd $WORKSPACE/build/src

# Download toolchain artifacts.
. taskcluster/scripts/misc/tooltool-download.sh

# We can't set the path to npm directly, but it's sibling to NODEJS.
export PATH=$PATH:`dirname $NODEJS`

# We don't install ImageMagick, so this will fail.  Continue.
./mach browsertime --setup || true

# We have tools/browsertime/{package.json,node_modules,...} and want
# browsertime/{package.json,node_modules}.  ZIP because generic-worker
# doesn't support .tar.xz.
mkdir -p  /builds/worker/artifacts
cd tools
zip -r /builds/worker/artifacts/browsertime.zip browsertime
