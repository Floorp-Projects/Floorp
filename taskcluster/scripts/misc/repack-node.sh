#!/bin/bash
set -x -e -v

# This script is for repacking Node (and NPM) from nodejs.org.

WORKSPACE=$HOME/workspace
SUFFIX=tar.xz
UNARCHIVE="tar xaf"
REPACK_TAR_COMPRESSION_SWITCH=J
REPACK_SUFFIX=tar.xz

case "$1" in
linux64)
    ARCH=linux-x64
    # From https://nodejs.org/dist/v8.17.0/SHASUMS256.txt.asc
    SHA256SUM=b7f6dd77fb173c8c7c30d61d0702eefc236bba74398538aa77bfa2bb47bddce6
    ;;
macosx64)
    ARCH=darwin-x64
    # From https://nodejs.org/dist/v8.17.0/SHASUMS256.txt.asc
    SHA256SUM=b6ef86df44292ba65f2b9a81b99a7db8de22a313f9c5abcebb6cf17ec24e2c97
    ;;
win64)
    ARCH=win-x64
    # From https://nodejs.org/dist/v8.17.0/SHASUMS256.txt.asc
    SHA256SUM=e95a63e81b27e78872c0efb9dd5809403014dbf9896035cc17adf51a350f88fa
    SUFFIX=zip
    UNARCHIVE=unzip
    REPACK_TAR_COMPRESSION_SWITCH=j
    REPACK_SUFFIX=tar.bz2
    ;;
win32)
    ARCH=win-x86
    # From https://nodejs.org/dist/v8.17.0/SHASUMS256.txt.asc
    SHA256SUM=3ecc0ab4c6ad957f5dfb9ca22453cd35908029fba86350fc96d070e8e5c213b5
    SUFFIX=zip
    UNARCHIVE=unzip
    REPACK_TAR_COMPRESSION_SWITCH=j
    REPACK_SUFFIX=tar.bz2
    ;;
esac

VERSION=8.17.0
# From https://nodejs.org/en/download/
URL=https://nodejs.org/dist/v$VERSION/node-v$VERSION-$ARCH.$SUFFIX
ARCHIVE=node-v$VERSION-$ARCH.$SUFFIX
DIR=node-v$VERSION

mkdir -p $UPLOAD_DIR

cd $WORKSPACE
wget --progress=dot:mega $URL

# shasum is available on both Linux and Windows builders, but on
# Windows, reading from stdin doesn't work as expected.
echo "$SHA256SUM  $ARCHIVE" > node.txt
shasum --algorithm 256 --check node.txt

$UNARCHIVE $ARCHIVE
mv node-v$VERSION-$ARCH node
tar c${REPACK_TAR_COMPRESSION_SWITCH}f $UPLOAD_DIR/node.$REPACK_SUFFIX node
