#!/bin/bash
# This script builds NSS with gyp and ninja.
#
# This build system is still under development.  It does not yet support all
# the features or platforms that NSS supports.
#
# -c = clean before build
# -g = force a rebuild of gyp (and NSPR, because why not)
# -v = verbose build
# --test = ignore map files and export everything we have

set -e

CWD=$(cd $(dirname $0); pwd -P)
OBJ_DIR=$(make -s -C "$CWD" platform)
DIST_DIR="$CWD/../dist/$OBJ_DIR"

if [ -n "$CCC" ] && [ -z "$CXX" ]; then
    export CXX="$CCC"
fi

while [ $# -gt 0 ]; do
    case $1 in
        -c) CLEAN=1 ;;
        -g) REBUILD_GYP=1 ;;
        -v) VERBOSE=1 ;;
        --test) GYP_PARAMS="$GYP_PARAMS -Dtest_build=1" ;;
    esac
    shift
done

# -c = clean first
if [ "$CLEAN" = 1 ]; then
    rm -rf "$CWD/out"
fi

if [ "$BUILD_OPT" = "1" ]; then
    TARGET=Release
else
    TARGET=Debug
fi
if [ "$USE_64" == "1" ]; then
    TARGET="${TARGET}_x64"
else
    GYP_PARAMS="$GYP_PARAMS -Dtarget_arch=ia32"
fi
TARGET_DIR="$CWD/out/$TARGET"

# These steps can take a while, so don't overdo them.
# Force a redo with -g.
if [ "$REBUILD_GYP" = 1 -o ! -d "$TARGET_DIR" ]; then
    # Build NSPR.
    make -C "$CWD" NSS_GYP=1 install_nspr

    # Run gyp.
    PKG_CONFIG_PATH="$CWD/../nspr/$OBJ_DIR/config" $SCANBUILD \
        gyp -f ninja $GYP_PARAMS --depth="$CWD" --generator-output="." "$CWD/nss.gyp"
fi

# Run ninja.
if which ninja >/dev/null 2>&1; then
    NINJA=ninja
elif which ninja-build >/dev/null 2>&1; then
    NINJA=ninja-build
else
    echo "Please install ninja" 1>&2
    exit 1
fi
if [ "$VERBOSE" = 1 ]; then
    NINJA="$NINJA -v"
fi
$NINJA -C "$TARGET_DIR"
