#!/bin/bash

set -x -e -v

# This script is for building clang-cl on Windows.

: TOOLTOOL_CACHE                ${TOOLTOOL_CACHE:=/builds/worker/tooltool-cache}
export TOOLTOOL_CACHE

TOOLTOOL_AUTH_FILE=/c/builds/relengapi.tok
if [ ! -e ${TOOLTOOL_AUTH_FILE} ]; then
    echo cannot find ${TOOLTOOL_AUTH_FILE}
    exit 1
fi
UPLOAD_PATH=$PWD/public/build

cd $GECKO_PATH

./mach artifact toolchain -v --authentication-file="${TOOLTOOL_AUTH_FILE}" --tooltool-manifest "${TOOLTOOL_MANIFEST}"${TOOLTOOL_CACHE:+ --cache-dir ${TOOLTOOL_CACHE}}${MOZ_TOOLCHAINS:+ ${MOZ_TOOLCHAINS}}

# Set up all the Visual Studio paths.
. taskcluster/scripts/misc/vs-setup.sh

# LLVM_ENABLE_DIA_SDK is set if the directory "$ENV{VSINSTALLDIR}DIA SDK"
# exists.
export VSINSTALLDIR="${VSPATH}/"

# Add git.exe to the path
export PATH="$(pwd)/cmd:${PATH}"
export PATH="$(cd cmake && pwd)/bin:${PATH}"
export PATH="$(cd ninja && pwd)/bin:${PATH}"

# gets a bit too verbose here
set +x

BUILD_CLANG_DIR=build/build-clang
cd ${BUILD_CLANG_DIR}
python3 ./build-clang.py -c ./${1}
cd -


set -x

# Put a tarball in the artifacts dir
mkdir -p ${UPLOAD_PATH}
cp ${BUILD_CLANG_DIR}/clang*.tar.* ${UPLOAD_PATH}
