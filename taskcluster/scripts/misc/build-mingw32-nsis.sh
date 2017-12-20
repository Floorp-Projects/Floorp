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

$GPG --import <<EOF
-----BEGIN PGP PUBLIC KEY BLOCK-----

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
pMCmSQ==
=5shY
-----END PGP PUBLIC KEY BLOCK-----
EOF

# --------------

download_and_check http://zlib.net/ zlib-1.2.11.tar.gz.asc
tar xaf $TMPDIR/zlib-1.2.11.tar.gz
cd zlib-1.2.11
make -f win32/Makefile.gcc PREFIX=i686-w64-mingw32-

cd ..
wget https://downloads.sourceforge.net/project/nsis/NSIS%203/3.01/nsis-3.01-src.tar.bz2
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
