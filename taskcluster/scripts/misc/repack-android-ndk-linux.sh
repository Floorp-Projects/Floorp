#!/bin/bash
set -x -e -v

# This script is for fetching and repacking the Android NDK (for
# Linux), the tools required to produce native Android programs.

UPLOAD_DIR=$HOME/project/gecko/android-ndk

mkdir -p $HOME/artifacts $UPLOAD_DIR

# Populate /builds/worker/.mozbuild/android-ndk-$VER.
cd $GECKO_PATH
./mach python python/mozboot/mozboot/android.py --ndk-only --no-interactive

# Don't generate a tarball with a versioned NDK directory.
mv $HOME/.mozbuild/android-ndk-* $HOME/.mozbuild/android-ndk
tar cv -C /builds/worker/.mozbuild android-ndk | $GECKO_PATH/taskcluster/scripts/misc/zstdpy > $UPLOAD_DIR/android-ndk.tar.zst

ls -al $UPLOAD_DIR
