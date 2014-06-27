#!/bin/bash
# A script to generate toolkit/components/downloads/csd.pb.{cc,h} for use in
# nsIApplicationReputationQuery. This script assumes you have downloaded and
# installed the protocol buffer compiler.

if [ -n $PROTOC_PATH ]; then
  PROTOC_PATH=/usr/local/bin/protoc
fi

echo "Using $PROTOC_PATH as protocol compiler"

if [ ! -e $PROTOC_PATH ]; then
  echo "You must install the protocol compiler from " \
       "https://code.google.com/p/protobuf/downloads/list"
  exit 1
fi

# Get the protocol buffer and compile it
CMD='wget http://src.chromium.org/chrome/trunk/src/chrome/common/safe_browsing/csd.proto -O csd.proto'
OUTPUT_PATH=toolkit/components/downloads

$CMD
$PROTOC_PATH csd.proto --cpp_out=$OUTPUT_PATH
