#!/bin/bash -vex

set -x -e

echo "running as" $(id)

: WORKSPACE ${WORKSPACE:=/builds/worker/workspace}

set -v

cd $WORKSPACE/build/src

# Download toolchain artifacts.
. taskcluster/scripts/misc/tooltool-download.sh

. taskcluster/scripts/misc/android-gradle-dependencies/before.sh

export MOZCONFIG=mobile/android/config/mozconfigs/android-api-16-gradle-dependencies/nightly
./mach build
./mach android gradle-dependencies

. taskcluster/scripts/misc/android-gradle-dependencies/after.sh
