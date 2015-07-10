#!/bin/bash
set -x -e -v

# This script is for building cctools (Apple's binutils) for Linux using
# crosstool-ng (https://github.com/diorcety/crosstool-ng).

WORKSPACE=$HOME/workspace
UPLOAD_DIR=$WORKSPACE/artifacts

# Repository info
: CROSSTOOL_NG_REPOSITORY    ${CROSSTOOL_NG_REPOSITORY:=https://github.com/diorcety/crosstool-ng}
: CROSSTOOL_NG_REV           ${CROSSTOOL_NG_HEAD_REV:=master}

# Set some crosstools-ng directories
CT_TOP_DIR=$WORKSPACE/crosstool-ng-build
CT_PREFIX_DIR=$WORKSPACE/cctools
CT_SRC_DIR=$CT_TOP_DIR/src
CT_TARBALLS_DIR=$CT_TOP_DIR
CT_WORK_DIR=$CT_SRC_DIR
CT_LIB_DIR=$WORKSPACE/crosstool-ng
CT_BUILD_DIR=$CT_TOP_DIR/build
CT_LLVM_DIR=$WORKSPACE/clang
CT_BUILDTOOLS_PREFIX_DIR=$CT_PREFIX_DIR

# Create our directories
rm -rf $CT_TOP_DIR
mkdir $CT_TOP_DIR
rm -rf $CT_PREFIX_DIR
mkdir $CT_PREFIX_DIR
mkdir -p $CT_SRC_DIR

# Work around one of the desktop-build hacks until we undo it.
mkdir -p /home/worker/workspace/build/src/gcc/bin/
ln -s `readlink -f /usr/bin/x86_64-linux-gnu-gcc.orig` /home/worker/workspace/build/src/gcc/bin/gcc

# Clone the crosstool-ng repo
tc-vcs checkout $CT_LIB_DIR $CROSSTOOL_NG_REPOSITORY $CROSSTOOL_NG_REPOSITORY $CROSSTOOL_NG_REV

# Fetch clang from tooltool
cd $WORKSPACE
wget -O tooltool.py https://raw.githubusercontent.com/mozilla/build-tooltool/master/tooltool.py
chmod +x tooltool.py
: TOOLTOOL_CACHE                ${TOOLTOOL_CACHE:=/home/worker/tooltool-cache}
export TOOLTOOL_CACHE
sh $WORKSPACE/build/tools/scripts/tooltool/tooltool_wrapper.sh $WORKSPACE/build/src/browser/config/tooltool-manifests/linux64/clang.manifest https://api.pub.build.mozilla.org/tooltool/ setup.sh $WORKSPACE/tooltool.py

# Copy clang into the crosstools-ng srcdir
cp -Rp $CT_LLVM_DIR $CT_SRC_DIR

# Configure crosstools-ng
sed=sed
CT_CONNECT_TIMEOUT=5
CT_BINUTILS_VERSION=809
CT_PATCH_ORDER=bundled
CT_BUILD=x86_64-linux-gnu
CT_HOST=x86_64-linux-gnu
CT_TARGET=x86_64-apple-darwin10
CT_LLVM_FULLNAME=clang

cd $CT_TOP_DIR

# gets a bit too verbose here
set +x

. $CT_LIB_DIR/scripts/functions
. $CT_LIB_DIR/scripts/build/binutils/cctools.sh

# Build cctools
do_binutils_get
do_binutils_extract
do_binutils_for_host

set -x

strip $CT_PREFIX_DIR/bin/*

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
tar czf $UPLOAD_DIR/cctools.tar.gz -C $WORKSPACE `basename $CT_PREFIX_DIR`
