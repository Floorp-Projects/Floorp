#!/usr/bin/env bash

set -v -e -x

hg clone https://hg.mozilla.org/build/tools

tools/scripts/tooltool/tooltool_wrapper.sh $(dirname $0)/releng.manifest https://api.pub.build.mozilla.org/tooltool/ non-existant-file.sh /c/mozilla-build/python/python.exe /c/builds/tooltool.py --authentication-file /c/builds/relengapi.tok -c /c/builds/tooltool_cache
VSPATH="$(pwd)/vs2015u2"

export WINDOWSSDKDIR="${VSPATH}/SDK"
export WIN32_REDIST_DIR="${VSPATH}/VC/redist/x64/Microsoft.VC140.CRT"
export WIN_UCRT_REDIST_DIR="${VSPATH}/SDK/Redist/ucrt/DLLs/x64"

export PATH="${VSPATH}/VC/bin/amd64:${VSPATH}/VC/bin:${VSPATH}/SDK/bin/x64:${VSPATH}/VC/redist/x64/Microsoft.VC140.CRT:${VSPATH}/SDK/Redist/ucrt/DLLs/x64:${VSPATH}/DIASDK/bin/amd64:${PATH}"

export INCLUDE="${VSPATH}/VC/include:${VSPATH}/VC/atlmfc/include:${VSPATH}/SDK/Include/ucrt:${VSPATH}/SDK/Include/shared:${VSPATH}/SDK/Include/um:${VSPATH}/SDK/Include/winrt:${VSPATH}/DIASDK/include"
export LIB="${VSPATH}/VC/lib/amd64:${VSPATH}/VC/atlmfc/lib/amd64:${VSPATH}/SDK/lib/ucrt/x64:${VSPATH}/SDK/lib/um/x64:${VSPATH}/DIASDK/lib/amd64"
