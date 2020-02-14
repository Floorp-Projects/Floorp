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
10) VERSION="10.19.0" ;;
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

    # From https://nodejs.org/dist/v10.19.0/SHASUMS256.txt.asc
    linux-x64--10.19.0)   SHA256SUM=34127c7c6b1ba02d6d4dc3a926f38a5fb88bb37fc7f051349005ce331c7a53c6 ;;
    darwin-x64--10.19.0)  SHA256SUM=91725d2ed64e4ccd265259e3e29a0e64a4d26d9d1cd9ba390e0cdec13ea7b02f ;;
    win-x64--10.19.0)     SHA256SUM=210efd45a7f79cf4c350d8f575f990becdd3833cd922796a4c83b27996f5679e ;;
    win-x86--10.19.0)     SHA256SUM=afd176d4f022b6a5dbd4a908d42c6d85d4f739c040f65430ab3bf60b8f3b9a96 ;;
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
