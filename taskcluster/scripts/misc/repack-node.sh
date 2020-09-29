#!/bin/bash
set -x -e -v

# This script is for repacking Node (and NPM) from nodejs.org.

WORKSPACE=$HOME/workspace

case "$1" in
linux64)  ARCH=linux-x64 ;;
macosx64) ARCH=darwin-x64 ;;
win64)    ARCH=win-x64 ;;
win32)    ARCH=win-x86 ;;
*)
    echo "Unknown architecture $1 not recognized in repack-node.sh" >&2
    exit 1
    ;;
esac

case "$ARCH" in
linux-x64|darwin-x64)
    SUFFIX=tar.xz
    UNARCHIVE="tar xaf"
    REPACK_TAR_COMPRESSION_SWITCH=J
    REPACK_SUFFIX=tar.xz
    ;;

win-x64|win-x86)
    SUFFIX=zip
    UNARCHIVE=unzip
    REPACK_TAR_COMPRESSION_SWITCH=j
    REPACK_SUFFIX=tar.bz2
    ;;
esac

# Although we're only using one version at the moment, this infrastructure is
# useful for when we need to do upgrades and have multiple versions of node
# live in taskcluster at once.
case "$2" in
10) VERSION="10.22.1" ;;
*)
    echo "Unknown version $2 not recognized in repack-node.sh" >&2
    exit 1
    ;;
esac

case "$ARCH--$VERSION" in
    # From https://nodejs.org/dist/v10.22.1/SHASUMS256.txt.asc
    linux-x64--10.22.1)   SHA256SUM=079d6329c7ba5da3e3fa0949b543e24e605daf985381b32ebd86df8d38f9afa6 ;;
    darwin-x64--10.22.1)  SHA256SUM=15eab0e90bbff02c73ce52a728ff0af5244d2c3c8a620df7d6df16e159326eab ;;
    win-x64--10.22.1)     SHA256SUM=2cc8c0080cf3c8e91b9c66845e369cedd29dd4afc027bdba775eadb6d7e2beda ;;
    win-x86--10.22.1)     SHA256SUM=37e34a3a3a02465f835dfae5372d0ba49be270a4362e43cbd94bca4b0d002265 ;;
esac

# From https://nodejs.org/en/download/
URL=https://nodejs.org/dist/v$VERSION/node-v$VERSION-$ARCH.$SUFFIX
ARCHIVE=node-v$VERSION-$ARCH.$SUFFIX

mkdir -p "$UPLOAD_DIR"

cd "$WORKSPACE"
wget --progress=dot:mega $URL

# shasum is available on both Linux and Windows builders, but on
# Windows, reading from stdin doesn't work as expected.
echo "$SHA256SUM  $ARCHIVE" > node.txt
shasum --algorithm 256 --check node.txt

$UNARCHIVE $ARCHIVE
mv node-v$VERSION-$ARCH node
# npx doesn't have great security characteristics (it downloads and executes
# stuff directly out of npm at runtime), so let's not risk it getting into
# anyone's PATH who doesn't already have it there:
rm -f node/bin/npx node/bin/npx.exe
tar c${REPACK_TAR_COMPRESSION_SWITCH}f "$UPLOAD_DIR"/node.$REPACK_SUFFIX node
