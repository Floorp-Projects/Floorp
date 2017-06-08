#!/usr/bin/env bash

set -v -e -x

source $(dirname $0)/setup.sh

export WIN32_REDIST_DIR="${VSPATH}/VC/redist/x86/Microsoft.VC140.CRT"
export WIN_UCRT_REDIST_DIR="${VSPATH}/SDK/Redist/ucrt/DLLs/x86"
export PATH="${NINJA_PATH}:${VSPATH}/VC/bin/amd64_x86:${VSPATH}/VC/bin/amd64:${VSPATH}/VC/bin:${VSPATH}/SDK/bin/x86:${VSPATH}/SDK/bin/x64:${VSPATH}/VC/redist/x86/Microsoft.VC140.CRT:${VSPATH}/VC/redist/x64/Microsoft.VC140.CRT:${VSPATH}/SDK/Redist/ucrt/DLLs/x86:${VSPATH}/SDK/Redist/ucrt/DLLs/x64:${PATH}"
export LIB="${VSPATH}/VC/lib:${VSPATH}/SDK/lib/10.0.14393.0/ucrt/x86:${VSPATH}/SDK/lib/10.0.14393.0/um/x86"
