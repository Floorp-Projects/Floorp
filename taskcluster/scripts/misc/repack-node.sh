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

case "$2" in
8)  VERSION="8.17.0" ;;
10) VERSION="10.18.1" ;;
*)
    echo "Unknown version $2 not recognized in repack-node.sh" >&2
    exit 1
    ;;
esac

case "$ARCH--$VERSION" in
    # From https://nodejs.org/dist/v8.17.0/SHASUMS256.txt.asc
    linux-x64--8.17.0)    SHA256SUM=b7f6dd77fb173c8c7c30d61d0702eefc236bba74398538aa77bfa2bb47bddce6 ;;
    darwin-x64--8.17.0)   SHA256SUM=b6ef86df44292ba65f2b9a81b99a7db8de22a313f9c5abcebb6cf17ec24e2c97 ;;
    win-x64--8.17.0)      SHA256SUM=e95a63e81b27e78872c0efb9dd5809403014dbf9896035cc17adf51a350f88fa ;;
    win-x86--8.17.0)      SHA256SUM=3ecc0ab4c6ad957f5dfb9ca22453cd35908029fba86350fc96d070e8e5c213b5 ;;

    # From https://nodejs.org/dist/v10.18.1/SHASUMS256.txt.asc
    linux-x64--10.18.1)   SHA256SUM=8cc40f45c2c62529b15e83a6bbe0ac1febf57af3c5720df68067c96c0fddbbdf ;;
    darwin-x64--10.18.1)  SHA256SUM=ea344da9fc5e07f1bdf5b192813d22b0e94d78e50bd7965711c01d99f094d9b0 ;;
    win-x64--10.18.1)     SHA256SUM=fb27bb95c27c72f2e25d0c41309b606b2ae48ba0d6094a19f206ad1df9dc5e19 ;;
    win-x86--10.18.1)     SHA256SUM=ffe874d6edfc56c88b85de118e14a2e999fa344e8814cc1e1d9cd4048dd75461 ;;
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
tar c${REPACK_TAR_COMPRESSION_SWITCH}f "$UPLOAD_DIR"/node.$REPACK_SUFFIX node
