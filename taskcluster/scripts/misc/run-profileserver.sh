#! /bin/bash -vex

set -x -e

echo "running as" $(id)

####
# Taskcluster friendly wrapper for running the profileserver
####

PGO_RUNDIR=/builds/worker/workspace/build/src/obj-firefox/dist
export UPLOAD_PATH=$HOME/artifacts
export JARLOG_FILE="en-US.log"

set -v

# run XVfb in the background
. /builds/worker/scripts/xvfb.sh

cleanup() {
    local rv=$?
    cleanup_xvfb
    exit $rv
}
trap cleanup EXIT INT

start_xvfb '1024x768x24' 2

cd /builds/worker/checkouts/gecko

# Move our fetched firefox into objdir/dist so the jarlog entries will match
# the paths when the final PGO stage packages the build.
mkdir -p $PGO_RUNDIR
mv $MOZ_FETCHES_DIR/firefox $PGO_RUNDIR
./mach python build/pgo/profileserver.py --binary $PGO_RUNDIR/firefox/firefox
tar -acvf $UPLOAD_PATH/profdata.tar.xz default.profraw en-US.log
