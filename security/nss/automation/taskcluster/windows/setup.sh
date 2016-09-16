#!/usr/bin/env bash

set -v -e -x

# Usage: hg_clone repo dir [revision=@]
hg_clone() {
    repo=$1
    dir=$2
    rev=${3:-@}
    for i in 0 2 5; do
        sleep $i
        hg clone -r "$rev" "$repo" "$dir" && return
        rm -rf "$dir"
    done
    exit 1
}

hg_clone https://hg.mozilla.org/build/tools tools default

tools/scripts/tooltool/tooltool_wrapper.sh $(dirname $0)/releng.manifest https://api.pub.build.mozilla.org/tooltool/ non-existant-file.sh /c/mozilla-build/python/python.exe /c/builds/tooltool.py --authentication-file /c/builds/relengapi.tok -c /c/builds/tooltool_cache
VSPATH="$(pwd)/vs2015u2"

export WINDOWSSDKDIR="${VSPATH}/SDK"
export WIN32_REDIST_DIR="${VSPATH}/VC/redist/x64/Microsoft.VC140.CRT"
export WIN_UCRT_REDIST_DIR="${VSPATH}/SDK/Redist/ucrt/DLLs/x64"

export PATH="${VSPATH}/VC/bin/amd64:${VSPATH}/VC/bin:${VSPATH}/SDK/bin/x64:${VSPATH}/VC/redist/x64/Microsoft.VC140.CRT:${VSPATH}/SDK/Redist/ucrt/DLLs/x64:${PATH}"

export INCLUDE="${VSPATH}/VC/include:${VSPATH}/SDK/Include/10.0.10586.0/ucrt:${VSPATH}/SDK/Include/10.0.10586.0/shared:${VSPATH}/SDK/Include/10.0.10586.0/um"
export LIB="${VSPATH}/VC/lib/amd64:${VSPATH}/SDK/lib/10.0.10586.0/ucrt/x64:${VSPATH}/SDK/lib/10.0.10586.0/um/x64"
