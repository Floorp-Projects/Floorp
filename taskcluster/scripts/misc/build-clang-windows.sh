#!/bin/bash

set -x -e -v

# This script is for building clang-cl on Windows.

# Fetch our toolchain from tooltool.
wget -O tooltool.py ${TOOLTOOL_REPO}/raw/${TOOLTOOL_REV}/tooltool.py
chmod +x tooltool.py
: TOOLTOOL_CACHE                ${TOOLTOOL_CACHE:=/home/worker/tooltool-cache}
export TOOLTOOL_CACHE

TOOLTOOL_AUTH_FILE=/c/builds/relengapi.tok
if [ ! -e ${TOOLTOOL_AUTH_FILE} ]; then
    echo cannot find ${TOOLTOOL_AUTH_FILE}
    exit 1
fi

TOOLTOOL_MANIFEST=build/src/browser/config/tooltool-manifests/win32/releng.manifest
./tooltool.py --authentication-file="${TOOLTOOL_AUTH_FILE}" -m "${TOOLTOOL_MANIFEST}" fetch

# gets a bit too verbose here
set +x

cd build/src/build/build-clang
/c/mozilla-build/python/python.exe ./build-clang.py -c clang-static-analysis-win32.json

set -x

# Put a tarball in the artifacts dir
mkdir -p ${UPLOAD_PATH}
cp clang.tar.* ${UPLOAD_PATH}
