#!/bin/bash
set -x -e -v

# This script is for fetching and repacking the Android SDK (for
# Linux), the tools required to produce Android packages.

mkdir -p $UPLOAD_DIR

# Populate /builds/worker/.mozbuild/android-sdk-linux.
cd $GECKO_PATH
./mach python python/mozboot/mozboot/android.py --artifact-mode --no-interactive --list-packages

# Bug 1869407: override emulator to a known working version
curl -L http://dl.google.com/android/repository/emulator-linux_x64-10696886.zip > /tmp/emulator.zip
cd /builds/worker/.mozbuild/android-sdk-linux
rm -rf emulator
unzip /tmp/emulator.zip
cd $GECKO_PATH

tar cavf $UPLOAD_DIR/android-sdk-linux.tar.zst -C /builds/worker/.mozbuild android-sdk-linux bundletool.jar

ls -al $UPLOAD_DIR
