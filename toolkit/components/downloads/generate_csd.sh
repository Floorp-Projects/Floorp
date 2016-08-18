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

if [ ! -f nsDownloadManager.cpp ]; then
    echo "You must run this script in the toolkit/components/downloads" >&2
    echo "directory of the source tree." >&2
    exit 1
fi

# Get the protocol buffer and compile it
CSD_PROTO_URL="https://chromium.googlesource.com/chromium/src/+/master/chrome/common/safe_browsing/csd.proto?format=TEXT"
CSD_PATH="chromium/chrome/common/safe_browsing"

curl "$CSD_PROTO_URL" | base64 --decode > "$CSD_PATH"/csd.proto
"$PROTOC_PATH" "$CSD_PATH"/csd.proto --cpp_out=.
