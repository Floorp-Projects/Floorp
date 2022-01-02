#!/bin/bash -vex

set -x -e

echo "running as" $(id)

set -v

cd $GECKO_PATH

# Needed for Nexus
export PATH=$JAVA_HOME/bin:$PATH

. taskcluster/scripts/misc/android-gradle-dependencies/before.sh

export MOZCONFIG=mobile/android/config/mozconfigs/android-arm-gradle-dependencies/nightly
./mach build
./mach android gradle-dependencies

. taskcluster/scripts/misc/android-gradle-dependencies/after.sh
