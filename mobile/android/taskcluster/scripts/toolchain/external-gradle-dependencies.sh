#!/bin/bash

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set -ex

function get_abs_path {
    local file_path="$1"
    echo "$( cd "$(dirname "$file_path")" >/dev/null 2>&1 ; pwd -P )"
}

CURRENT_DIR="$(get_abs_path $0)"
REPO_ROOT_DIR="$(get_abs_path $CURRENT_DIR/../../../..)"
ANDROID_COMPONENTS_DIR="$REPO_ROOT_DIR/android-components"
WORKING_DIR="$REPO_ROOT_DIR/$1"
shift
GRADLE_COMMANDS="$@"

NEXUS_PREFIX='http://localhost:8081/nexus/content/repositories'
REPOS="-PgoogleRepo=$NEXUS_PREFIX/google/ -PcentralRepo=$NEXUS_PREFIX/central/"
GRADLE_ARGS="--parallel $REPOS -Pcoverage"

pushd "$WORKING_DIR"

. "$REPO_ROOT_DIR/taskcluster/scripts/toolchain/external-gradle-dependencies/before.sh"

# Before building anything we explicitly build one component that contains Glean and initializes
# the Miniconda Python environment and doesn't have (almost) any other transitive dependencies.
# If that happens concurrently with other tasks then this seems to fail quite often. So let's do it
# here first and also not use the "--parallel` flag.
./gradlew $REPOS support-sync-telemetry:assemble

# Plugins aren't automatically built. That's why we build them one by one here
if [[ $WORKING_DIR == ${ANDROID_COMPONENTS_DIR}* ]]; then
  for plugin_dir in $(find "$ANDROID_COMPONENTS_DIR/plugins" -mindepth 1 -maxdepth 1 -type d); do
    pushd "$plugin_dir"
    "$ANDROID_COMPONENTS_DIR/gradlew" $GRADLE_ARGS assemble test
    popd
  done
fi

./gradlew $GRADLE_ARGS $GRADLE_COMMANDS

. "$REPO_ROOT_DIR/taskcluster/scripts/toolchain/external-gradle-dependencies/after.sh"

popd
