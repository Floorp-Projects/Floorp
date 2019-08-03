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

INSTALL_DIR=$GECKO_PATH/mingw32

mkdir -p $INSTALL_DIR

cd $GECKO_PATH
. taskcluster/scripts/misc/tooltool-download.sh
# After tooltool runs, we move the stuff we just downloaded.
# As explained above, we have to build nsis to the directory it
# will eventually be run from, which is the same place we just
# installed our compiler. But at the end of the script we want
# to package up what we just built. If we don't move the compiler,
# we will package up the compiler we downloaded along with the
# stuff we just built.
mv $MOZ_FETCHES_DIR/mingw32 mingw32-gcc
export PATH="$GECKO_PATH/mingw32-gcc/bin:$PATH"

cd $MOZ_FETCHES_DIR

# --------------

cd zlib-1.2.11
make -f win32/Makefile.gcc PREFIX=i686-w64-mingw32-

cd ../nsis-3.01-src
# I don't know how to make the version work with the environment variables/config flags the way the author appears to
sed -i "s/'VERSION', 'Version of NSIS', cvs_version/'VERSION', 'Version of NSIS', '3.01'/" SConstruct
scons XGCC_W32_PREFIX=i686-w64-mingw32- ZLIB_W32=../zlib-1.2.11 SKIPUTILS="NSIS Menu" PREFIX=$INSTALL_DIR/ install

# --------------

cd $GECKO_PATH
tar caf nsis.tar.xz mingw32

mkdir -p $UPLOAD_DIR
cp nsis.tar.* $UPLOAD_DIR
