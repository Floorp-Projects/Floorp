#!/bin/bash

set -xe

: ${TOOLTOOL_SERVER:=https://tooltool.mozilla-releng.net/}
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
        TOOLTOOL_AUTH_FILE=/builds/relengapi.tok
        ;;
    linux-gnu)
        PLATFORM_OS=linux
        TOOLTOOL_AUTH_FILE=/builds/relengapi.tok
        ;;
    msys)
        PLATFORM_OS=win
        TOOLTOOL_AUTH_FILE=c:/builds/relengapi.tok
        ;;
    *)
        echo "Unrecognized OSTYPE '$OSTYPE'" >&2
        PLATFORM_OS=linux
        ;;
esac

TOOLTOOL_AUTH_FLAGS=

if [ -e "$TOOLTOOL_AUTH_FILE" ]; then
    # When the worker has the relengapi token pass it down
    TOOLTOOL_AUTH_FLAGS="--authentication-file=$TOOLTOOL_AUTH_FILE"
fi

# Install everything needed for the browser on this platform. Not all of it is
# necessary for the JS shell, but it's less duplication to share tooltool
# manifests.
BROWSER_PLATFORM=$PLATFORM_OS$BITS

: ${TOOLTOOL_CHECKOUT:=$WORK}
export TOOLTOOL_CHECKOUT

(cd $TOOLTOOL_CHECKOUT && ${SRCDIR}/mach artifact toolchain${TOOLTOOL_MANIFEST:+ -v $TOOLTOOL_AUTH_FLAGS --tooltool-url $TOOLTOOL_SERVER --tooltool-manifest $SRCDIR/$TOOLTOOL_MANIFEST}${TOOLTOOL_CACHE:+ --cache-dir $TOOLTOOL_CACHE}${MOZ_TOOLCHAINS:+ ${MOZ_TOOLCHAINS}})

# Add all the tooltool binaries to our $PATH.
for bin in $TOOLTOOL_CHECKOUT/*/bin $TOOLTOOL_CHECKOUT/VC/bin/Hostx64/x86; do
    export PATH="$bin:$PATH"
done
