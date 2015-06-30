#!/bin/bash

set -x

: SPIDERMONKEY_VARIANT ${SPIDERMONKEY_VARIANT:-plain}

UPLOAD_DIR=$HOME/workspace/artifacts/

# cd into the correct directory
cd $HOME/workspace/

# Run the script
./build/src/js/src/devtools/automation/autospider.sh $SPIDERMONKEY_VARIANT
BUILD_STATUS=$?

# Ensure upload dir exists
mkdir -p $UPLOAD_DIR

# Move artifacts for upload by TaskCluster
mv ./build/src/obj-spider/dist/* $UPLOAD_DIR

exit $BUILD_STATUS
