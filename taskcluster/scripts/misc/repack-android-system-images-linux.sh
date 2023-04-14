#!/bin/bash
set -x -e -v

# This script is for fetching and repacking the Android SDK (for
# Linux), the tools required to produce Android packages.

AVD_JSON_CONFIG="$1"

mkdir -p $UPLOAD_DIR

# Populate /builds/worker/.mozbuild/android-sdk-linux.
cd $GECKO_PATH
./mach python python/mozboot/mozboot/android.py --artifact-mode --system-images-only --avd-manifest="$AVD_JSON_CONFIG" --no-interactive --list-packages

tar cavf $UPLOAD_DIR/android-system-images-linux.tar.zst -C /builds/worker/.mozbuild android-sdk-linux/system-images

ls -al $UPLOAD_DIR
