#!/bin/bash
#
# This script builds minidump_stackwalk binaries from the Google Breakpad
# source for all of the operating systems that we run Firefox tests on:
# Linux x86, Linux x86-64, Windows x86, OS X x86-64.
#
# It expects to be run in the luser/breakpad-builder:0.3 Docker image and
# needs access to the relengapiproxy to download internal tooltool files.

set -v -e -x

: BREAKPAD_REPO        ${BREAKPAD_REPO:=https://google-breakpad.googlecode.com/svn/trunk/}
: BREAKPAD_REV         ${BREAKPAD_REV:=HEAD}

function build()
{
    cd /tmp
    local platform=$1
    local configure_args=$2
    local make_args=$3
    local objdir=/tmp/obj-breakpad-$platform
    local install_dir=/tmp/install-breakpad-$platform
    local ext=
    if test "$platform" = "win32"; then
        ext=.exe
    fi
    rm -rf $objdir
    mkdir $objdir
    rm -rf $install_dir
    mkdir $install_dir
    cd $objdir
    CFLAGS=-O2 CXXFLAGS=-O2 /tmp/google-breakpad/configure --prefix=$install_dir --disable-tools $configure_args
    make -j`grep -c ^processor /proc/cpuinfo` $make_args
    make install-strip $make_args
    cp $install_dir/bin/minidump_stackwalk* /tmp/stackwalker/${platform}-minidump_stackwalk${ext}
}

function linux64()
{
    export LDFLAGS="-static"
    build linux64
    unset LDFLAGS
}

function linux32()
{
    export LDFLAGS="-static"
    build linux32 "--enable-m32"
    unset LDFLAGS
}

function macosx64()
{
    cd /tmp
    python tooltool.py -v --manifest=macosx-sdk.manifest --url=http://relengapi/tooltool/ fetch
    tar xjf MacOSX10.7.sdk.tar.bz2
    local FLAGS="-target x86_64-apple-darwin10 -mlinker-version=136 -B /tmp/cctools/bin -isysroot /tmp/MacOSX10.7.sdk"
    export CC="clang $FLAGS"
    export CXX="clang++ $FLAGS"
    local old_path=$PATH
    export PATH=$PATH:/tmp/cctools/bin/
    export LD_LIBRARY_PATH=/usr/lib/llvm-3.5/lib/

    build macosx64 "--host=x86_64-apple-darwin10" "AR=/tmp/cctools/bin/x86_64-apple-darwin10-ar"

    unset CC CXX LD_LIBRARY_PATH
    export PATH=$old_path
}

function win32()
{
    export LDFLAGS="-static-libgcc -static-libstdc++"
    build win32 "--host=i686-w64-mingw32"
    unset LDFLAGS
}

cd /tmp
mkdir -p stackwalker
if ! test -d google-breakpad; then
    svn checkout -r $BREAKPAD_REV $BREAKPAD_REPO google-breakpad
else
    (cd google-breakpad; svn update -r $BREAKPAD_REV)
fi
(cd google-breakpad; svn info)
linux64
linux32
macosx64
win32
