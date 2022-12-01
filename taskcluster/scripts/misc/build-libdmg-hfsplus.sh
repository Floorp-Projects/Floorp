#!/bin/bash
set -x -e -v

# This script is for building libdmg-hfsplus to get the `dmg` and `hfsplus`
# tools for producing DMG archives on Linux.

WORKSPACE=$HOME/workspace
STAGE=$WORKSPACE/dmg

mkdir -p $UPLOAD_DIR $STAGE

cd $MOZ_FETCHES_DIR/libdmg-hfsplus

# The openssl libraries in the sysroot cannot be linked in a PIE executable so we use -no-pie
cmake \
  -DCMAKE_C_COMPILER=$MOZ_FETCHES_DIR/clang/bin/clang \
  -DCMAKE_CXX_COMPILER=$MOZ_FETCHES_DIR/clang/bin/clang++ \
  -DCMAKE_SYSROOT=$MOZ_FETCHES_DIR/sysroot \
  -DOPENSSL_USE_STATIC_LIBS=1 \
  -DCMAKE_EXE_LINKER_FLAGS=-no-pie \
  .

make VERBOSE=1 -j$(nproc)

# We only need the dmg and hfsplus tools.
strip dmg/dmg hfs/hfsplus
cp dmg/dmg hfs/hfsplus $STAGE

# duplicate the functionality of taskcluster-lib-urls, but in bash..
queue_base="$TASKCLUSTER_ROOT_URL/api/queue/v1"

cat >$STAGE/README<<EOF
Source is available as a taskcluster artifact:
$queue_base/task/$(python -c 'import json, os; print "{task}/artifacts/{artifact}".format(**next(f for f in json.loads(os.environ["MOZ_FETCHES"]) if "libdmg-hfsplus" in f["artifact"]))')
EOF
tar caf $UPLOAD_DIR/dmg.tar.zst -C $WORKSPACE `basename $STAGE`
