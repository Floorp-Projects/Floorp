#!/bin/bash

set -xe

: ${TOOLTOOL_SERVER:=https://api.pub.build.mozilla.org/tooltool/}
: ${SPIDERMONKEY_VARIANT:=plain}
: ${UPLOAD_DIR:=$HOME/artifacts/}
: ${WORK:=$HOME/workspace}
: ${SRCDIR:=$WORK/build/src}

mkdir -p $WORK
cd $WORK

# Need to install things from tooltool. Figure out what platform to use.

case $(uname -m) in
    i686 | arm )
        BITS=32
        ;;
    *)
        BITS=64
        ;;
esac

case "$OSTYPE" in
    darwin*)
        PLATFORM_OS=macosx
        ;;
    linux-gnu)
        PLATFORM_OS=linux
        ;;
    msys)
        PLATFORM_OS=win
        ;;
    *)
        echo "Unrecognized OSTYPE '$OSTYPE'" >&2
        PLATFORM_OS=linux
        ;;
esac

# Install everything needed for the browser on this platform. Not all of it is
# necessary for the JS shell, but it's less duplication to share tooltool
# manifests.
BROWSER_PLATFORM=$PLATFORM_OS$BITS

: ${TOOLTOOL_CHECKOUT:=$WORK}
export TOOLTOOL_CHECKOUT

(cd $TOOLTOOL_CHECKOUT && ${SRCDIR}/mach artifact toolchain -v --tooltool-url $TOOLTOOL_SERVER --tooltool-manifest $SRCDIR/$TOOLTOOL_MANIFEST ${TOOLTOOL_CACHE:+ --cache-dir $TOOLTOOL_CACHE}${MOZ_TOOLCHAINS:+ ${MOZ_TOOLCHAINS}})
