#!/bin/bash
set -x -e -v

# This script is for fetching and repacking the Android emulator (for
# Linux), the tools required to produce Android packages.

UPLOAD_DIR=$HOME/project/gecko/android-emulator

mkdir -p $HOME/artifacts $UPLOAD_DIR

# Populate /builds/worker/.mozbuild/android-emulator-linux.
cd $GECKO_PATH
./mach python python/mozboot/mozboot/android.py --emulator-only --no-interactive

# Remove extra files we don't need
rm -rfv /builds/worker/.mozbuild/android-sdk-linux/tools
mkdir /builds/worker/.mozbuild/android-sdk-linux/system-images
mkdir /builds/worker/.mozbuild/android-sdk-linux/platforms
find /builds/worker/.mozbuild/android-sdk-linux/emulator/qemu -type f -not -name "*x86*" -print -delete

tar cavf $UPLOAD_DIR/android-emulator-linux.tar.zst -C /builds/worker/.mozbuild android-sdk-linux

ls -al $UPLOAD_DIR
