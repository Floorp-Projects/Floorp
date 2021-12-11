#!/bin/bash

set -xe

# Default variables values.
: ${WORK:=$HOME/workspace}

mkdir -p $UPLOAD_DIR

# Package up the sources into the release tarball.
AUTOMATION=1 DIST=$UPLOAD_DIR $GECKO_PATH/js/src/make-source-package.py

# Extract the tarball into a new directory in the workspace.

PACKAGE_DIR=$WORK/sm-package

# Do not use -p option because the package directory should not exist.
mkdir $PACKAGE_DIR
pushd $PACKAGE_DIR

tar -xvf $UPLOAD_DIR/mozjs-*.tar.*z*

: ${PYTHON3:=python3}

status=0
(
    # Build the freshly extracted, packaged SpiderMonkey.
    cd ./mozjs-*

    ./mach create-mach-environment
    AUTOMATION=1 $PYTHON3 js/src/devtools/automation/autospider.py --skip-tests=checks $SPIDERMONKEY_VARIANT
) || status=$?

# Copy artifacts for upload by TaskCluster
cp -rL ./mozjs-*/obj-spider/dist/bin/{js,jsapi-tests,js-gdb.py,libmozjs*} $UPLOAD_DIR

exit $status
