#!/bin/bash
#
# This script builds minidump_stackwalk binaries from the Google Breakpad
# source for all of the operating systems that we run Firefox tests on:
# Linux x86, Linux x86-64, Windows x86, OS X x86-64.
#
# It expects to be run in the luser/breakpad-builder:0.5 Docker image and
# needs access to the relengapiproxy to download internal tooltool files.

set -v -e -x

: BREAKPAD_REPO        ${BREAKPAD_REPO:=https://google-breakpad.googlecode.com/svn/trunk/}
: BREAKPAD_REV         ${BREAKPAD_REV:=HEAD}
: STACKWALK_HTTP_REPO  ${STACKWALK_HTTP_REPO:=https://hg.mozilla.org/users/tmielczarek_mozilla.com/stackwalk-http}
: STACKWALK_HTTP_REV   ${STACKWALK_HTTP_REV:=default}

ncpu=`grep -c ^processor /proc/cpuinfo`

function build()
{
    cd /tmp
    local platform=$1
    local strip_prefix=$2
    local configure_args=$3
    local make_args=$4
    local objdir=/tmp/obj-breakpad-$platform
    local ext=
    if test "$platform" = "win32"; then
        ext=.exe
    fi
    rm -rf $objdir
    mkdir $objdir
    # First, build Breakpad
    cd $objdir
    CFLAGS="-O2 $CFLAGS" CXXFLAGS="-O2 $CXXFLAGS" /tmp/google-breakpad/configure --disable-tools $configure_args
    make -j$ncpu $make_args src/libbreakpad.a src/third_party/libdisasm/libdisasm.a src/processor/stackwalk_common.o
    # Second, build stackwalk-http
    make -f /tmp/stackwalk-http/Makefile BREAKPAD_SRCDIR=/tmp/google-breakpad BREAKPAD_OBJDIR=`pwd` OS=$platform -j$ncpu
    ${strip_prefix}strip stackwalk${ext}
    cp stackwalk${ext} /tmp/stackwalker/${platform}-minidump_stackwalk${ext}
}

function linux64()
{
    export LDFLAGS="-static-libgcc -static-libstdc++"
    build linux64
    unset LDFLAGS
}

function linux32()
{
    export LDFLAGS="-static-libgcc -static-libstdc++ -L/tmp/libcurl-i386/lib"
    export CFLAGS="-m32 -I/tmp/libcurl-i386/include"
    export CXXFLAGS="-m32 -I/tmp/libcurl-i386/include"
    build linux32 "" "--enable-m32"
    unset LDFLAGS CFLAGS CXXFLAGS
}

function macosx64()
{
    cd /tmp
    python tooltool.py -v --manifest=macosx-sdk.manifest --url=http://relengapi/tooltool/ fetch
    tar xjf MacOSX10.6.sdk.tar.bz2
    export MACOSX_SDK=/tmp/MacOSX10.6.sdk
    export CCTOOLS=/tmp/cctools
    local FLAGS="-target x86_64-apple-darwin10 -mlinker-version=136 -B /tmp/cctools/bin -isysroot ${MACOSX_SDK} -mmacosx-version-min=10.6"
    export CC="clang $FLAGS"
    export CXX="clang++ $FLAGS -std=c++11"
    local old_path=$PATH
    export PATH=$PATH:/tmp/cctools/bin/
    export LD_LIBRARY_PATH=/usr/lib/llvm-3.6/lib/

    build macosx64 "/tmp/cctools/bin/x86_64-apple-darwin10-" "--host=x86_64-apple-darwin10" "AR=/tmp/cctools/bin/x86_64-apple-darwin10-ar"

    unset CC CXX LD_LIBRARY_PATH MACOSX_SDK CCTOOLS
    export PATH=$old_path
}

function win32()
{
    export LDFLAGS="-static-libgcc -static-libstdc++"
    export CFLAGS="-D__USE_MINGW_ANSI_STDIO=1"
    export CXXFLAGS="-D__USE_MINGW_ANSI_STDIO=1"
    export ZLIB_DIR=/tmp/zlib-mingw
    build win32 "i686-w64-mingw32-" "--host=i686-w64-mingw32"
    unset LDFLAGS CFLAGS CXXFLAGS ZLIB_DIR
}

cd /tmp
mkdir -p stackwalker
if ! test -d google-breakpad; then
    svn checkout -r $BREAKPAD_REV $BREAKPAD_REPO google-breakpad
fi
(cd google-breakpad; svn info)
if ! test -d stackwalk-http; then
  hg clone -u $STACKWALK_HTTP_REV $STACKWALK_HTTP_REPO
fi
linux64
linux32
macosx64
win32
