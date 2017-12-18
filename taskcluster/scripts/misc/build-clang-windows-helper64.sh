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

./build/src/mach artifact toolchain -v --authentication-file="${TOOLTOOL_AUTH_FILE}" --tooltool-manifest "build/src/${TOOLTOOL_MANIFEST}"${TOOLTOOL_CACHE:+ --cache-dir ${TOOLTOOL_CACHE}}${MOZ_TOOLCHAINS:+ ${MOZ_TOOLCHAINS}}

# Set up all the Visual Studio paths.
MSVC_DIR=vs2017_15.4.2
VSWINPATH="$(cd ${MSVC_DIR} && pwd)"

echo vswinpath ${VSWINPATH}

# LLVM_ENABLE_DIA_SDK is set if the directory "$ENV{VSINSTALLDIR}DIA SDK"
# exists.
export VSINSTALLDIR="${VSWINPATH}/"

export WINDOWSSDKDIR="${VSWINPATH}/SDK"
export WIN32_REDIST_DIR="${VSWINPATH}/VC/redist/x64/Microsoft.VC141.CRT"
export WIN_UCRT_REDIST_DIR="${VSWINPATH}/SDK/Redist/ucrt/DLLs/x64"

export PATH="${VSWINPATH}/VC/bin/Hostx64/x64:${VSWINPATH}/SDK/bin/10.0.15063.0/x64:${VSWINPATH}/VC/redist/x64/Microsoft.VC141.CRT:${VSWINPATH}/SDK/Redist/ucrt/DLLs/x64:${VSWINPATH}/DIA SDK/bin/amd64:${PATH}"

export INCLUDE="${VSWINPATH}/VC/include:${VSWINPATH}/VC/atlmfc/include:${VSWINPATH}/SDK/Include/10.0.15063.0/ucrt:${VSWINPATH}/SDK/Include/10.0.15063.0/shared:${VSWINPATH}/SDK/Include/10.0.15063.0/um:${VSWINPATH}/SDK/Include/10.0.15063.0/winrt:${VSWINPATH}/DIA SDK/include"
export LIB="${VSWINPATH}/VC/lib/x64:${VSWINPATH}/VC/atlmfc/lib/x64:${VSWINPATH}/SDK/Lib/10.0.15063.0/ucrt/x64:${VSWINPATH}/SDK/Lib/10.0.15063.0/um/x64:${VSWINPATH}/DIA SDK/lib/amd64"

export PATH="$(cd svn && pwd)/bin:${PATH}"
export PATH="$(cd cmake && pwd)/bin:${PATH}"
export PATH="$(cd ninja && pwd)/bin:${PATH}"

# We use |mach python| to set up a virtualenv automatically for us.  We create
# a dummy mozconfig, because the default machinery for config.guess-choosing
# of the objdir doesn't work very well.
MOZCONFIG="$(pwd)/mozconfig"
cat > ${MOZCONFIG} <<EOF
mk_add_options MOZ_OBJDIR=$(pwd)/objdir
EOF

# gets a bit too verbose here
set +x

BUILD_CLANG_DIR=build/src/build/build-clang
cd ${BUILD_CLANG_DIR}
MOZCONFIG=${MOZCONFIG} ../../mach python ./build-clang.py -c ./${1}
cd -


set -x

# Put a tarball in the artifacts dir
UPLOAD_PATH=public/build
mkdir -p ${UPLOAD_PATH}
cp ${BUILD_CLANG_DIR}/clang*.tar.* ${UPLOAD_PATH}
