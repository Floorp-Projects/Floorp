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
PROJECT_DIR="$(get_abs_path $CURRENT_DIR/../../../..)"

pushd $PROJECT_DIR

. taskcluster/scripts/toolchain/android-gradle-dependencies/before.sh

NEXUS_PREFIX='http://localhost:8081/nexus/content/repositories'
GRADLE_ARGS="--parallel -PgoogleRepo=$NEXUS_PREFIX/google/ -PcentralRepo=$NEXUS_PREFIX/central/"
# We build everything to be sure to fetch all dependencies
./gradlew $GRADLE_ARGS assemble assembleAndroidTest testClasses ktlint detekt
# Some tests may be flaky, although they still download dependencies. So we let the following
# command fail, if needed.
set +e; ./gradlew $GRADLE_ARGS -Pcoverage test mozilla-detekt-rules:test mozilla-lint-rules:test; set -e


# ./gradlew lint is missing because of https://github.com/mozilla-mobile/fenix/issues/10439. So far,
# we're lucky and the dependencies it fetches are found elsewhere.

. taskcluster/scripts/toolchain/android-gradle-dependencies/after.sh

popd
