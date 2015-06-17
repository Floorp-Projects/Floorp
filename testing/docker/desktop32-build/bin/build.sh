#! /bin/bash -vex

set -x -e -v

####
# The default build works for any fx_desktop_build based mozharness job:
# via linux-build.sh
####

. $HOME/bin/checkout-sources.sh

. $WORKSPACE/build/src/testing/taskcluster/scripts/builder/build-linux.sh
