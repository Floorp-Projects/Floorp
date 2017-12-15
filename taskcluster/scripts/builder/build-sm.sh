#!/bin/bash

set -x

source $(dirname $0)/sm-tooltool-config.sh

: ${PYTHON:=python2.7}

# Run the script
export MOZ_UPLOAD_DIR="$UPLOAD_DIR"
AUTOMATION=1 $PYTHON $SRCDIR/js/src/devtools/automation/autospider.py $SPIDERMONKEY_VARIANT
BUILD_STATUS=$?

# Ensure upload dir exists
mkdir -p $UPLOAD_DIR

# Copy artifacts for upload by TaskCluster
cp -rL $SRCDIR/obj-spider/dist/bin/{js,jsapi-tests,js-gdb.py} $UPLOAD_DIR

# Fuzzing users would really like to have llvm-symbolizer available in the same
# directory as the built output.
gzip -c $TOOLTOOL_CHECKOUT/clang/bin/llvm-symbolizer > $UPLOAD_DIR/llvm-symbolizer.gz || true

exit $BUILD_STATUS
