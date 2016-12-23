#!/bin/bash

set -x -e -v

# This script is for building clang-cl on Windows.

chmod +x build/src/taskcluster/docker/recipes/tooltool.py
: TOOLTOOL_CACHE                ${TOOLTOOL_CACHE:=/home/worker/tooltool-cache}
export TOOLTOOL_CACHE

TOOLTOOL_AUTH_FILE=/c/builds/relengapi.tok
if [ ! -e ${TOOLTOOL_AUTH_FILE} ]; then
    echo cannot find ${TOOLTOOL_AUTH_FILE}
    exit 1
fi

TOOLTOOL_MANIFEST=build/src/browser/config/tooltool-manifests/win32/build-clang-cl.manifest
./build/src/taskcluster/docker/recipes/tooltool.py --authentication-file="${TOOLTOOL_AUTH_FILE}" -m "${TOOLTOOL_MANIFEST}" fetch

# Set up all the Visual Studio paths.
MSVC_DIR=vs2015u3
VSWINPATH="$(cd ${MSVC_DIR} && pwd)"

echo vswinpath ${VSWINPATH}

export WINDOWSSDKDIR="${VSWINPATH}/SDK"
export WIN32_REDIST_DIR="${VSWINPATH}/VC/redist/x86/Microsoft.VC140.CRT"
export WIN_UCRT_REDIST_DIR="${VSWINPATH}/SDK/Redist/ucrt/DLLs/x86"

export PATH="${VSWINPATH}/VC/bin/amd64_x86:${VSWINPATH}/VC/bin/amd64:${VSWINPATH}/VC/bin:${VSWINPATH}/SDK/bin/x86:${VSWINPATH}/SDK/bin/x64:${VSWINPATH}/DIA SDK/bin:${PATH}"
export PATH="${VSWINPATH}/VC/redist/x86/Microsoft.VC140.CRT:${VSWINPATH}/VC/redist/x64/Microsoft.VC140.CRT:${VSWINPATH}/SDK/Redist/ucrt/DLLs/x86:${VSWINPATH}/SDK/Redist/ucrt/DLLs/x64:${PATH}"

export INCLUDE="${VSWINPATH}/VC/include:${VSWINPATH}/VC/atlmfc/include:${VSWINPATH}/SDK/Include/10.0.14393.0/ucrt:${VSWINPATH}/SDK/Include/10.0.14393.0/shared:${VSWINPATH}/SDK/Include/10.0.14393.0/um:${VSWINPATH}/SDK/Include/10.0.14393.0/winrt:${VSWINPATH}/DIA SDK/include"
export LIB="${VSWINPATH}/VC/lib:${VSWINPATH}/VC/atlmfc/lib:${VSWINPATH}/SDK/lib/10.0.14393.0/ucrt/x86:${VSWINPATH}/SDK/lib/10.0.14393.0/um/x86:${VSWINPATH}/DIA SDK/lib"

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
MOZCONFIG=${MOZCONFIG} build/src/mach python ${BUILD_CLANG_DIR}/build-clang.py -c ${BUILD_CLANG_DIR}/clang-static-analysis-win32.json

set -x

# Put a tarball in the artifacts dir
UPLOAD_PATH=public/build
mkdir -p ${UPLOAD_PATH}
cp clang.tar.* ${UPLOAD_PATH}
