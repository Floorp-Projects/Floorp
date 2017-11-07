#!/bin/bash
set -e -v

# This script is for building GN on Windows.

WORKSPACE=$PWD
UPLOAD_DIR=$WORKSPACE/public/build
COMPRESS_EXT=bz2

VSPATH="$WORKSPACE/build/src/vs2017_15.4.2"

export INCLUDE="${VSPATH}/VC/include:${VSPATH}/VC/atlmfc/include:${VSPATH}/SDK/Include/10.0.15063.0/ucrt:${VSPATH}/SDK/Include/10.0.15063.0/shared:${VSPATH}/SDK/Include/10.0.15063.0/um:${VSPATH}/SDK/Include/10.0.15063.0/winrt:${VSPATH}/DIA SDK/include"
export LIB="${VSPATH}/VC/lib/x86:${VSPATH}/VC/atlmfc/lib/x86:${VSPATH}/SDK/lib/10.0.15063.0/ucrt/x86:${VSPATH}/SDK/lib/10.0.15063.0/um/x86:${VSPATH}/DIA SDK/lib"

export PATH="$WORKSPACE/build/src/ninja/bin:$PATH"
export PATH="$WORKSPACE/build/src/mingw64/bin:$PATH"
export PATH="${VSPATH}/VC/bin/Hostx64/x86:${VSPATH}/VC/bin/Hostx64/x64:${VSPATH}/VC/bin/Hostx86/x86:${VSPATH}/SDK/bin/10.0.15063.0/x64:${VSPATH}/DIA SDK/bin:${PATH}"
export PATH="${VSPATH}/VC/redist/x86/Microsoft.VC141.CRT:${VSPATH}/SDK/Redist/ucrt/DLLs/x86:${PATH}"

cd $WORKSPACE/build/src

. taskcluster/scripts/misc/tooltool-download.sh
. taskcluster/scripts/misc/build-gn-common.sh
