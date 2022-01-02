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
export LLVM_PROFDATA=$MOZ_FETCHES_DIR/clang/bin/llvm-profdata

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

tar -acvf $UPLOAD_PATH/profdata.tar.xz merged.profdata en-US.log
