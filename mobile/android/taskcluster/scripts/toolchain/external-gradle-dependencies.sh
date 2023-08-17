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

: WORKSPACE ${WORKSPACE:=/builds/worker/workspace}
NEXUS_PREFIX='http://localhost:8081/nexus/content/repositories'
REPOS="-PgoogleRepo=$NEXUS_PREFIX/google/ -PcentralRepo=$NEXUS_PREFIX/central/"
GRADLE_ARGS="--parallel $REPOS -Pcoverage"

# override the default org.gradle.jvmargs to add more heap space
GRADLE_USER_HOME="${WORKSPACE}/gradle-home"
mkdir -p "${GRADLE_USER_HOME}"
export GRADLE_USER_HOME
echo "org.gradle.jvmargs=-Xmx16g -Xms2g -XX:MaxMetaspaceSize=6g -XX:+HeapDumpOnOutOfMemoryError -XX:+UseParallelGC" > "${GRADLE_USER_HOME}/gradle.properties"

pushd "$WORKING_DIR"

. "$REPO_ROOT_DIR/taskcluster/scripts/toolchain/external-gradle-dependencies/before.sh"

# Before building anything we explicitly build one component that contains Glean and initializes
# the Miniconda Python environment and doesn't have (almost) any other transitive dependencies.
# If that happens concurrently with other tasks then this seems to fail quite often. So let's do it
# here first and also not use the "--parallel` flag.
./gradlew $REPOS service-nimbus:assemble

# Plugins aren't automatically built. That's why we build them one by one here
if [[ $WORKING_DIR == ${ANDROID_COMPONENTS_DIR}* ]]; then
  for plugin_dir in $(find "$ANDROID_COMPONENTS_DIR/plugins" -mindepth 1 -maxdepth 1 -type d); do
    pushd "$plugin_dir"
    "$ANDROID_COMPONENTS_DIR/gradlew" $GRADLE_ARGS assemble test
    popd
  done
fi

# gradle occasionally hangs, so run a watchdog that'll exit after 30min
(
sleep 30m
exit 42
) &

./gradlew $GRADLE_ARGS $GRADLE_COMMANDS &
# wait for either the watchdog or gradle process to exit
wait -n %1 %2
# if gradle finished in time, kill the watchdog, no longer necessary
kill %1 || :

. "$REPO_ROOT_DIR/taskcluster/scripts/toolchain/external-gradle-dependencies/after.sh"

popd
