#! /bin/bash -vex

set -x -e

echo "running as" $(id)

: NEED_XVFB     ${NEED_XVFB:=false}
: UPLOAD_PATH   ${UPLOAD_PATH:=$HOME/artifacts}
export UPLOAD_PATH

####
# Taskcluster friendly wrapper for running the profileserver
####

PGO_RUNDIR=obj-firefox/dist
export JARLOG_FILE="en-US.log"

set -v

if $NEED_XVFB; then
    # run XVfb in the background
    . /builds/worker/scripts/xvfb.sh

    cleanup() {
        local rv=$?
        cleanup_xvfb
        exit $rv
    }
    trap cleanup EXIT INT

    start_xvfb '1024x768x24' 2
fi

# Move our fetched firefox into objdir/dist so the jarlog entries will match
# the paths when the final PGO stage packages the build.
mkdir -p $PGO_RUNDIR
mkdir -p $UPLOAD_PATH
mv $MOZ_FETCHES_DIR/firefox $PGO_RUNDIR
./mach python build/pgo/profileserver.py --binary $PGO_RUNDIR/firefox/firefox

# Fail the build if for some reason we didn't collect any profile data.
if test -z "$(find . -maxdepth 1 -name '*.profraw' -print -quit)"; then
    echo "ERROR: no profile data produced"
    exit 1
fi

tar -acvf $UPLOAD_PATH/profdata.tar.xz *.profraw en-US.log
