#! /bin/bash -vex
set -x -e

####
# Taskcluster friendly wrapper for running the profileserver on macOS
####

export UPLOAD_PATH=../../artifacts
mkdir -p $UPLOAD_PATH

export JARLOG_FILE="en-US.log"

export LLVM_PROFDATA=$MOZ_FETCHES_DIR/clang/bin/llvm-profdata

set -v

./mach python python/mozbuild/mozbuild/action/install.py $MOZ_FETCHES_DIR/target.dmg $MOZ_FETCHES_DIR
./mach python build/pgo/profileserver.py --binary $MOZ_FETCHES_DIR/*.app/Contents/MacOS/firefox

tar -Jcvf $UPLOAD_PATH/profdata.tar.xz merged.profdata en-US.log
