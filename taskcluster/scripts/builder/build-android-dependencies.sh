#!/bin/bash -vex

set -x -e

echo "running as" $(id)

: WORKSPACE ${WORKSPACE:=/builds/worker/workspace}

set -v

. $WORKSPACE/build/src/taskcluster/scripts/builder/build-android-dependencies/before.sh

. $WORKSPACE/build/src/taskcluster/scripts/builder/build-linux.sh

. $WORKSPACE/build/src/taskcluster/scripts/builder/build-android-dependencies/after.sh
