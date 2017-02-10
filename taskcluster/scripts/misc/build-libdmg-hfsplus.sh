#!/bin/bash
set -x -e -v

# This script is for building libdmg-hfsplus to get the `dmg` and `hfsplus`
# tools for producing DMG archives on Linux.

WORKSPACE=$HOME/workspace
STAGE=$WORKSPACE/dmg
UPLOAD_DIR=$WORKSPACE/artifacts

# There's no single well-maintained fork of libdmg-hfsplus, but this
# branch currently has some fixes we need.
: LIBDMG_REPOSITORY    ${LIBDMG_REPOSITORY:=https://github.com/andreas56/libdmg-hfsplus}
# This is the current head of the `from_zarvox` branch.
: LIBDMG_REV           ${LIBDMG_REV:=81dd75fd1549b24bf8af9736ac25518b367e6b63}

mkdir -p $UPLOAD_DIR $STAGE

cd $WORKSPACE
tc-vcs checkout --force-clone libdmg-hfsplus $LIBDMG_REPOSITORY $LIBDMG_REPOSITORY $LIBDMG_REV
cd libdmg-hfsplus
# Make a source archive
git archive ${LIBDMG_REV} | xz > $UPLOAD_DIR/libdmg-hfsplus.tar.xz
cmake .
make -j$(getconf _NPROCESSORS_ONLN)

# We only need the dmg and hfsplus tools.
strip dmg/dmg hfs/hfsplus
cp dmg/dmg hfs/hfsplus $STAGE

cat >$STAGE/README<<EOF
Built from ${LIBDMG_REPOSITORY} rev `git rev-parse ${LIBDMG_REV}`.
Source is available in tooltool, digest `sha512sum $UPLOAD_DIR/libdmg-hfsplus.tar.xz`.
EOF
tar cf - -C $WORKSPACE `basename $STAGE` | xz > $UPLOAD_DIR/dmg.tar.xz
