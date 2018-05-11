#!/bin/bash
set -x -e -v

# We set the INSTALL_DIR to match the directory that it will run in exactly,
# otherwise we get an NSIS error of the form:
#   checking for NSIS version...
#   DEBUG: Executing: `/home/worker/workspace/build/src/mingw32/
#   DEBUG: The command returned non-zero exit status 1.
#   DEBUG: Its error output was:
#   DEBUG: | Error: opening stub "/home/worker/workspace/mingw32/
#   DEBUG: | Error initalizing CEXEBuild: error setting
#   ERROR: Failed to get nsis version.

WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
INSTALL_DIR=$WORKSPACE/build/src/mingw32
TOOLTOOL_DIR=$WORKSPACE/build/src
UPLOAD_DIR=$HOME/artifacts

mkdir -p $INSTALL_DIR

root_dir=$HOME_DIR
data_dir=$HOME_DIR/src/build/unix/build-gcc

. $data_dir/download-tools.sh

cd $TOOLTOOL_DIR
. taskcluster/scripts/misc/tooltool-download.sh
# After tooltool runs, we move the stuff we just downloaded.
# As explained above, we have to build nsis to the directory it
# will eventually be run from, which is the same place we just
# installed our compiler. But at the end of the script we want
# to package up what we just built. If we don't move the compiler,
# we will package up the compiler we downloaded along with the
# stuff we just built.
mv mingw32 mingw32-gcc
export PATH="$TOOLTOOL_DIR/mingw32-gcc/bin:$PATH"

cd $WORKSPACE

$GPG --import $data_dir/5ED46A6721D365587791E2AA783FCD8E58BCAFBA.key

# --------------

download_and_check http://zlib.net/ zlib-1.2.11.tar.gz.asc
tar xaf $TMPDIR/zlib-1.2.11.tar.gz
cd zlib-1.2.11
make -f win32/Makefile.gcc PREFIX=i686-w64-mingw32-

cd ..
wget --progress=dot:mega https://downloads.sourceforge.net/project/nsis/NSIS%203/3.01/nsis-3.01-src.tar.bz2
echo "559703cc25f78697be1784a38d1d9a19c97d27a200dc9257d1483c028c6be9242cbcd10391ba618f88561c2ba57fdbd8b3607bea47ed8c3ad7509a6ae4075138  nsis-3.01-src.tar.bz2" | sha512sum -c -
bunzip2 nsis-3.01-src.tar.bz2
tar xaf nsis-3.01-src.tar
cd nsis-3.01-src
# I don't know how to make the version work with the environment variables/config flags the way the author appears to
sed -i "s/'VERSION', 'Version of NSIS', cvs_version/'VERSION', 'Version of NSIS', '3.01'/" SConstruct
scons XGCC_W32_PREFIX=i686-w64-mingw32- ZLIB_W32=../zlib-1.2.11 SKIPUTILS="NSIS Menu" PREFIX=$INSTALL_DIR/ install

# --------------

cd $WORKSPACE/build/src
tar caf nsis.tar.xz mingw32

mkdir -p $UPLOAD_DIR
cp nsis.tar.* $UPLOAD_DIR
