#!/bin/bash

set -x

: SPIDERMONKEY_VARIANT ${SPIDERMONKEY_VARIANT:-plain}

UPLOAD_DIR=$HOME/artifacts/

# cd into the correct directory
cd $HOME/workspace/

# Run the script
./build/src/js/src/devtools/automation/autospider.sh $SPIDERMONKEY_VARIANT
BUILD_STATUS=$?

# Ensure upload dir exists
mkdir -p $UPLOAD_DIR

# Copy artifacts for upload by TaskCluster
cp -rL ./build/src/obj-spider/dist/bin/{js,jsapi-tests,js-gdb.py} $UPLOAD_DIR

exit $BUILD_STATUS
