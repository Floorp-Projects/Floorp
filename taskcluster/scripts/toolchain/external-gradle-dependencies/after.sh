#!/bin/bash

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This is inspired by
# https://searchfox.org/mozilla-central/rev/2cd2d511c0d94a34fb7fa3b746f54170ee759e35/taskcluster/scripts/misc/android-gradle-dependencies/after.sh.
# gradle-plugins was removed because it's not used in this project.

set -x -e -v

echo "running as $(id)"

: WORKSPACE "${WORKSPACE:=/builds/worker/workspace}"
ARTIFACTS_TARGET_DIR='/builds/worker/artifacts'
EXTERNAL_DEPS='external-gradle-dependencies'
NEXUS_STORAGE_DIR="$NEXUS_WORK/storage"
NEXUS_DIRS="$NEXUS_STORAGE_DIR/google $NEXUS_STORAGE_DIR/central"
BAD_DOWNLOADS_FILE="$WORKSPACE/bad_downloads.txt"
BAD_DOWNLOADS_EXIT_CODE=17


function _package_artifacts_downloaded_by_nexus() {
    pushd "$WORKSPACE"

    mkdir -p "$WORKSPACE/$EXTERNAL_DEPS" "$ARTIFACTS_TARGET_DIR"
    for nexus_dir in $NEXUS_DIRS; do
        cp -R "$nexus_dir" "$EXTERNAL_DEPS"
    done

    tar cf - "$EXTERNAL_DEPS" | xz > "$ARTIFACTS_TARGET_DIR/$EXTERNAL_DEPS.tar.xz"
    popd
}


function _ensure_artifacts_are_sane() {
    # Let's find empty files or unfinished downloads
    find "$WORKSPACE/$EXTERNAL_DEPS" -size 0 -o -name '*.part' > "$BAD_DOWNLOADS_FILE"

    if [ -s "$BAD_DOWNLOADS_FILE" ]; then
        echo "ERROR: Some artifacts were not correctly downloaded! Please look at:"
        cat "$BAD_DOWNLOADS_FILE"
        exit $BAD_DOWNLOADS_EXIT_CODE
    fi
}


_package_artifacts_downloaded_by_nexus
_ensure_artifacts_are_sane
