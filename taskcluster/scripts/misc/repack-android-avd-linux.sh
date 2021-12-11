#!/bin/bash
set -x -e -v

# Initialize XVFB for the AVD
. /builds/worker/scripts/xvfb.sh

cleanup() {
    local rv=$?
    cleanup_xvfb
    exit $rv
}
trap cleanup EXIT INT

start_xvfb '1024x768x24' 2

# This script is for fetching and repacking the Android SDK (for
# Linux), the tools required to produce Android packages.

UPLOAD_DIR=/builds/worker/artifacts/
AVD_JSON_CONFIG="$1"

mkdir -p $HOME/artifacts $UPLOAD_DIR

# Populate /builds/worker/.mozbuild/android-device
cd $GECKO_PATH
./mach python python/mozboot/mozboot/android.py --artifact-mode --prewarm-avd --avd-manifest="$AVD_JSON_CONFIG" --no-interactive --list-packages

tar cavf $UPLOAD_DIR/android-avd-linux.tar.zst -C /builds/worker/.mozbuild android-device

ls -al $UPLOAD_DIR
