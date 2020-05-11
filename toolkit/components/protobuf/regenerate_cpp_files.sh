#!/usr/bin/env bash

set -e

if which protoc >/dev/null ; then
    echo "Using $(which protoc) to regenerate .pb.cc and .pb.h files."
else
    echo "protoc not found in your path. Cannot regenerate the .pb.cc and .pb.h files."
    exit 1
fi

regenerate() {
    DIR="$1"
    PROTO="$2"
    echo
    echo "${DIR}${PROTO}:"
    pushd "$DIR" >/dev/null
    protoc --cpp_out=. "$PROTO"
    popd >/dev/null
}

cd $(dirname $0)
cd ../../.. # Top level.

regenerate gfx/layers/protobuf/ LayerScopePacket.proto
regenerate devtools/shared/heapsnapshot/ CoreDump.proto
regenerate toolkit/components/reputationservice/chromium/chrome/common/safe_browsing/ csd.proto
regenerate toolkit/components/url-classifier/chromium/ safebrowsing.proto
