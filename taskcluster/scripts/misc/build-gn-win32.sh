#!/bin/bash
set -e -v

# This script is for building GN on Windows.

WORKSPACE=$PWD
UPLOAD_DIR=$WORKSPACE/public/build
COMPRESS_EXT=bz2

VSPATH="$WORKSPACE/build/src/vs2015u3"

export INCLUDE="${VSPATH}/VC/include:${VSPATH}/VC/atlmfc/include:${VSPATH}/SDK/Include/10.0.14393.0/ucrt:${VSPATH}/SDK/Include/10.0.14393.0/shared:${VSPATH}/SDK/Include/10.0.14393.0/um:${VSPATH}/SDK/Include/10.0.14393.0/winrt:${VSPATH}/DIA SDK/include"
export LIB="${VSPATH}/VC/lib:${VSPATH}/VC/atlmfc/lib:${VSPATH}/SDK/lib/10.0.14393.0/ucrt/x86:${VSPATH}/SDK/lib/10.0.14393.0/um/x86:${VSPATH}/DIA SDK/lib"

export PATH="$WORKSPACE/build/src/ninja/bin:$PATH"
export PATH="$WORKSPACE/build/src/mingw64/bin:$PATH"
export PATH="${VSPATH}/VC/bin/amd64_x86:${VSPATH}/VC/bin/amd64:${VSPATH}/VC/bin:${VSPATH}/SDK/bin/x86:${VSPATH}/SDK/bin/x64:${VSPATH}/DIA SDK/bin:${PATH}"
export PATH="${VSPATH}/VC/redist/x86/Microsoft.VC140.CRT:${VSPATH}/VC/redist/x64/Microsoft.VC140.CRT:${VSPATH}/SDK/Redist/ucrt/DLLs/x86:${VSPATH}/SDK/Redist/ucrt/DLLs/x64:${PATH}"

cd $WORKSPACE/build/src

. taskcluster/scripts/misc/tooltool-download.sh
. taskcluster/scripts/misc/build-gn-common.sh
