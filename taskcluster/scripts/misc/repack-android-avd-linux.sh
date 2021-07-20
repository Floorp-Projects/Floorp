#!/bin/bash
set -x -e -v

# This script is for fetching and repacking the Android SDK (for
# Linux), the tools required to produce Android packages.

UPLOAD_DIR=$HOME/project/gecko/android-avd
AVD_JSON_CONFIG="$1"

mkdir -p $HOME/artifacts $UPLOAD_DIR

# Populate /builds/worker/.mozbuild/android-device
cd $GECKO_PATH
./mach python python/mozboot/mozboot/android.py --artifact-mode --avd-manifest="$AVD_JSON_CONFIG" --no-interactive

tar cavf $UPLOAD_DIR/android-avd-linux.tar.zst -C /builds/worker/.mozbuild android-device

ls -al $UPLOAD_DIR
