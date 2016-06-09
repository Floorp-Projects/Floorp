#!/bin/bash

set -x

source $(dirname $0)/sm-tooltool-config.sh

# Run the script
AUTOMATION=1 $SRCDIR/js/src/devtools/automation/autospider.sh $SPIDERMONKEY_VARIANT
BUILD_STATUS=$?

# Ensure upload dir exists
mkdir -p $UPLOAD_DIR

# Copy artifacts for upload by TaskCluster
cp -rL $SRCDIR/obj-spider/dist/bin/{js,jsapi-tests,js-gdb.py} $UPLOAD_DIR

exit $BUILD_STATUS
