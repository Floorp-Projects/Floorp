#!/bin/bash

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This is copy of
# https://searchfox.org/mozilla-central/rev/2cd2d511c0d94a34fb7fa3b746f54170ee759e35/taskcluster/scripts/misc/android-gradle-dependencies/after.sh.
# gradle-plugins was removed because it's not used in this project.

set -x -e

echo "running as" $(id)

: WORKSPACE ${WORKSPACE:=/builds/worker/workspace}

set -v

# Package everything up.
pushd $WORKSPACE
mkdir -p android-gradle-dependencies /builds/worker/artifacts

cp -R ${NEXUS_WORK}/storage/google android-gradle-dependencies
cp -R ${NEXUS_WORK}/storage/central android-gradle-dependencies

tar cf - android-gradle-dependencies | xz > /builds/worker/artifacts/android-gradle-dependencies.tar.xz

popd
