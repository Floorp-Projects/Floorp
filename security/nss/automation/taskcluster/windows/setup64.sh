#!/usr/bin/env bash

set -v -e -x

source $(dirname $0)/setup.sh

export WIN32_REDIST_DIR="${VSPATH}/VC/redist/x64/Microsoft.VC141.CRT"
export WIN_UCRT_REDIST_DIR="${VSPATH}/SDK/Redist/ucrt/DLLs/x64"
export PATH="${NINJA_PATH}:${VSPATH}/VC/bin/Hostx64/x64:${VSPATH}/VC/bin/Hostx86/x86:${VSPATH}/SDK/bin/10.0.15063.0/x64:${VSPATH}/VC/redist/x64/Microsoft.VC141.CRT:${VSPATH}/SDK/Redist/ucrt/DLLs/x64:${PATH}"
export LIB="${VSPATH}/VC/lib/x64:${VSPATH}/SDK/lib/10.0.15063.0/ucrt/x64:${VSPATH}/SDK/lib/10.0.15063.0/um/x64"
