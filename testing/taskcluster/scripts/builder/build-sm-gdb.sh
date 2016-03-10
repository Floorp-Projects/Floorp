#!/bin/bash

# Run Spidermonkey's gdb integration tests

set -x

. $(dirname $0)/sm-setup.sh

: ${SPIDERMONKEY_VARIANT:=plaindebug}

cd $WORK

# Run the script

$SRCDIR/js/src/devtools/automation/autospider.sh --skip-test all --test gdb $SPIDERMONKEY_VARIANT
BUILD_STATUS=$?

# Ensure upload dir exists
mkdir -p $UPLOAD_DIR

# Copy artifacts for upload by TaskCluster
cp -rL $SRCDIR/obj-spider/dist/bin/{gdb-tests,gdb-tests-gdb.py,js-gdb.py} $UPLOAD_DIR

exit $BUILD_STATUS
