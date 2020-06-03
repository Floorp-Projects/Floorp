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
10) VERSION="10.21.0" ;;
*)
    echo "Unknown version $2 not recognized in repack-node.sh" >&2
    exit 1
    ;;
esac

case "$ARCH--$VERSION" in
    # From https://nodejs.org/dist/v10.21.0/SHASUMS256.txt.asc
    linux-x64--10.21.0)   SHA256SUM=1d3296763e46540047099e4910812e81c4899c0595f2d82474e2099c1e1603e2 ;;
    darwin-x64--10.21.0)  SHA256SUM=36eec82a6cd881a937c94a2d03136c9836b39e254ab325840426acd5f3c5787c ;;
    win-x64--10.21.0)     SHA256SUM=03dddcdaccdb40978ddf15d189acdc20409d9a666636db2595118690ff83ce82 ;;
    win-x86--10.21.0)     SHA256SUM=1ed92df4f39d183b0dee5205813ba9a32b8aff3e6f55d7f4d65193659dbad248 ;;
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
