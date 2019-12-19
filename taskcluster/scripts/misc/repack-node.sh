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
    # From https://nodejs.org/dist/v8.11.3/SHASUMS256.txt.asc
    SHA256SUM=08e2fcfea66746bd966ea3a89f26851f1238d96f86c33eaf6274f67fce58421a
    ;;
macosx64)
    ARCH=darwin-x64
    # From https://nodejs.org/dist/v8.11.3/SHASUMS256.txt.asc
    SHA256SUM=7eac0bf398cb6ecf9f84dfc577ee84eee3d930f7a54b7e50f56d1a358b528792
    ;;
win64)
    ARCH=win-x64
    # From https://nodejs.org/dist/v8.11.3/SHASUMS256.txt.asc
    SHA256SUM=91b779def1b21dcd1def7fc9671a869a1e2f989952e76fdc08a5d73570075f31
    SUFFIX=zip
    UNARCHIVE=unzip
    REPACK_TAR_COMPRESSION_SWITCH=j
    REPACK_SUFFIX=tar.bz2
    ;;
win32)
    ARCH=win-x86
    # From https://nodejs.org/dist/v8.11.3/SHASUMS256.txt.asc
    SHA256SUM=9482a0ad7aa5cd964cbeb11a605377b5c5aae4eae952c838aecf079de6088dc6
    SUFFIX=zip
    UNARCHIVE=unzip
    REPACK_TAR_COMPRESSION_SWITCH=j
    REPACK_SUFFIX=tar.bz2
    ;;
esac

VERSION=8.11.3
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
