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

hg_clone https://hg.mozilla.org/build/tools tools b8d7c263dfc3
tools/scripts/tooltool/tooltool_wrapper.sh \
    $(dirname $0)/releng.manifest http://taskcluster/tooltool.mozilla-releng.net/ \
    non-existant-file.sh /c/mozilla-build/python/python.exe \
    /c/builds/tooltool.py \
    -c /c/builds/tooltool_cache

# This needs $m to be set.
[[ -n "$m" ]]

# Setup MSVC paths.
export VSPATH="${PWD}/vs2017_15.4.2"
UCRTVersion="10.0.15063.0"

export WINDOWSSDKDIR="${VSPATH}/SDK"
export VS90COMNTOOLS="${VSPATH}/VC"
export WIN32_REDIST_DIR="${VSPATH}/VC/redist/${m}/Microsoft.VC141.CRT"
export WIN_UCRT_REDIST_DIR="${VSPATH}/SDK/Redist/ucrt/DLLs/${m}"

if [ "$m" == "x86" ]; then
    PATH="${PATH}:${VSPATH}/VC/bin/Hostx64/x86"
    PATH="${PATH}:${VSPATH}/VC/bin/Hostx64/x64"
fi
PATH="${PATH}:${VSPATH}/VC/bin/Host${m}/${m}"
PATH="${PATH}:${WIN32_REDIST_DIR}"
PATH="${PATH}:${WIN_UCRT_REDIST_DIR}"
PATH="${PATH}:${VSPATH}/SDK/bin/${UCRTVersion}/x64"
export PATH

LIB="${LIB}:${VSPATH}/VC/lib/${m}"
LIB="${LIB}:${VSPATH}/SDK/lib/${UCRTVersion}/ucrt/${m}"
LIB="${LIB}:${VSPATH}/SDK/lib/${UCRTVersion}/um/${m}"
export LIB

INCLUDE="${INCLUDE}:${VSPATH}/VC/include"
INCLUDE="${INCLUDE}:${VSPATH}/SDK/Include/${UCRTVersion}/ucrt"
INCLUDE="${INCLUDE}:${VSPATH}/SDK/Include/${UCRTVersion}/shared"
INCLUDE="${INCLUDE}:${VSPATH}/SDK/Include/${UCRTVersion}/um"
export INCLUDE
