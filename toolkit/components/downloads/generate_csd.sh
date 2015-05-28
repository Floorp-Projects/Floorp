#!/usr/bin/env bash

# A script to generate toolkit/components/downloads/csd.pb.{cc,h} for use in
# nsIApplicationReputationQuery. This script assumes you have downloaded and
# installed the protocol buffer compiler.
# As of June 26 2014, csd.proto contains many protobufs that are currently
# unused by ApplicationReputation. You may want to strip csd.proto of these
# before running the protocol compiler on it.

set -e

if [ "${PROTOC_PATH:+set}" != "set" ]; then
  PROTOC_PATH=/usr/local/bin/protoc
fi

echo "Using $PROTOC_PATH as protocol compiler"

if [ ! -e $PROTOC_PATH ]; then
  echo "You must install the protocol compiler from " \
       "https://code.google.com/p/protobuf/downloads/list"
  exit 1
fi

if [ ! -f nsDownloadManager.cpp ]; then
    echo "You must run this script in the toolkit/components/downloads" >&2
    echo "directory of the source tree." >&2
    exit 1
fi

# Get the protocol buffer and compile it
CSD_PROTO_URL="https://chromium.googlesource.com/playground/chromium-blink-merge/+/master/chrome/common/safe_browsing/csd.proto?format=TEXT"

curl $CSD_PROTO_URL | base64 --decode > csd.proto
$PROTOC_PATH csd.proto --cpp_out=.
