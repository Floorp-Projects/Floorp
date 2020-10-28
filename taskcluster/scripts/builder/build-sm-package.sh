#!/bin/bash

set -xe

source $(dirname $0)/sm-tooltool-config.sh

mkdir -p $UPLOAD_DIR

# Package up the sources into the release tarball.
AUTOMATION=1 DIST=$UPLOAD_DIR $GECKO_PATH/js/src/make-source-package.sh

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
    cd ./mozjs-*/js/src

    # MOZ_AUTOMATION enforces certain requirements that don't apply to
    # packaged builds. Unset it.
    unset MOZ_AUTOMATION

    AUTOMATION=1 $PYTHON3 ./devtools/automation/autospider.py --skip-tests=checks $SPIDERMONKEY_VARIANT
) || status=$?

# Copy artifacts for upload by TaskCluster
cp -rL ./mozjs-*/obj-spider/dist/bin/{js,jsapi-tests,js-gdb.py,libmozjs*} $UPLOAD_DIR

exit $status
