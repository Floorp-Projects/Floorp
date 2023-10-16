#!/usr/bin/env bash

set -e

usage() {
    echo "Usage: upgrade_protobuf.sh path/to/protobuf"
    echo
    echo "    Upgrades mozilla-central's copy of the protobuf library."
    echo
    echo "    Get a protobuf release from here:"
    echo "    https://github.com/google/protobuf/releases"
}

if [[ "$#" -ne 1 ]]; then
    usage
    exit 1
fi

PROTOBUF_LIB_PATH=$1

if [[ ! -d "$PROTOBUF_LIB_PATH" ]]; then
    echo No such directory: $PROTOBUF_LIB_PATH
    echo
    usage
    exit 1
fi

realpath() {
    if [[ $1 = /* ]]; then
        echo "$1"
    else
        echo "$PWD/${1#./}"
    fi
}

PROTOBUF_LIB_PATH=$(realpath $PROTOBUF_LIB_PATH)

cd $(dirname $0)

# Remove the old protobuf sources.
rm -rf src/google/*

# Add all the new protobuf sources.
cp -r $PROTOBUF_LIB_PATH/src/google/* src/google/

# Remove compiler sources.
rm -rf src/google/protobuf/compiler

# Remove test files.
find src/google -name '*_unittest*' | xargs rm -f
find src/google -name '*unittest_*' | xargs rm -f
find src/google -name 'unittest.*' | xargs rm -f
find src/google -name '*_test*' | xargs rm -f
find src/google -name '*test_*' | xargs rm -f
find src/google -type d -name 'testdata' | xargs rm -rf
find src/google -type d -name 'testing' | xargs rm -rf

# Remove protobuf's build files.
find src/google/ -name '.deps' | xargs rm -rf
find src/google/ -name '.dirstamp' | xargs rm -rf
rm -rf src/google/protobuf/SEBS

# Apply custom changes for building as part of mozilla-central.

cd ../../.. # Top level.

echo
echo Applying custom changes for mozilla-central. If this fails, you need to
echo edit the 'toolkit/components/protobuf/src/google/*' sources manually and
echo update the 'toolkit/components/protobuf/m-c-changes.patch' patch file
echo accordingly.
echo

echo
echo Successfully upgraded the protobuf lib!
