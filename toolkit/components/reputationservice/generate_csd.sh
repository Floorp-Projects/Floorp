#!/usr/bin/env bash

# A script to generate
# chromium/chrome/common/safe_browsing/csd.pb.{cc,h} for use in
# nsIApplicationReputationQuery. This script assumes you have
# downloaded and installed the protocol buffer compiler.

set -e

if [ "${PROTOC_PATH:+set}" != "set" ]; then
  PROTOC_PATH=/usr/local/bin/protoc
fi

echo "Using $PROTOC_PATH as protocol compiler"

if [ ! -e $PROTOC_PATH ]; then
  echo "You must install the protocol compiler from " \
       "https://github.com/google/protobuf/releases"
  exit 1
fi

if [ ! -f nsIApplicationReputation.idl ]; then
    echo "You must run this script in the toolkit/components/reputationservice" >&2
    echo "directory of the source tree." >&2
    exit 1
fi

# Get the protocol buffer and compile it
CSD_PROTO_URL="https://chromium.googlesource.com/chromium/src/+/main/components/safe_browsing/core/common/proto/csd.proto?format=TEXT"
CSD_PATH="chromium/chrome/common/safe_browsing"

# Switch to directory with csd.proto before compiling it
pushd "$CSD_PATH" >/dev/null
curl "$CSD_PROTO_URL" | base64 --decode > csd.proto
"$PROTOC_PATH" csd.proto --cpp_out=.
popd >/dev/null
