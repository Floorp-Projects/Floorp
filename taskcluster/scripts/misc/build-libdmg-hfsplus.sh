#!/bin/bash
set -x -e -v

# This script is for building libdmg-hfsplus to get the `dmg` and `hfsplus`
# tools for producing DMG archives on Linux.

WORKSPACE=$HOME/workspace
STAGE=$WORKSPACE/dmg

mkdir -p $UPLOAD_DIR $STAGE

cd $MOZ_FETCHES_DIR/libdmg-hfsplus

cmake -DOPENSSL_USE_STATIC_LIBS=1 .
make -j$(nproc)

# We only need the dmg and hfsplus tools.
strip dmg/dmg hfs/hfsplus
cp dmg/dmg hfs/hfsplus $STAGE

# duplicate the functionality of taskcluster-lib-urls, but in bash..
if [ "$TASKCLUSTER_ROOT_URL" = "https://taskcluster.net" ]; then
    queue_base='https://queue.taskcluster.net/v1'
else
    queue_base="$TASKCLUSTER_ROOT_URL/api/queue/v1"
fi

cat >$STAGE/README<<EOF
Source is available as a taskcluster artifact:
$queue_base/task/$(python -c 'import json, os; print "{task}/artifacts/{artifact}".format(**next(f for f in json.loads(os.environ["MOZ_FETCHES"]) if "libdmg-hfsplus" in f["artifact"]))')
EOF
tar cf - -C $WORKSPACE `basename $STAGE` | xz > $UPLOAD_DIR/dmg.tar.xz
