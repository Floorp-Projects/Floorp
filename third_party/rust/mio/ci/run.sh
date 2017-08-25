#!/bin/sh

# Builds and runs tests for a particular target passed as an argument to this
# script.

set -ex

TARGET=$1

if [ -f /etc/cargo_config ] && [ -d /cargo ]; then cp -f /etc/cargo_config /cargo/config; fi
cargo build --target=$TARGET --test test --verbose

# Find the file to run
TEST_FILE=$(find target/$TARGET/debug -maxdepth 1 -type f -perm -111 -name "test-*" | head -1)

case "$TARGET" in
  arm-linux-androideabi)
    # Use the 64bit emulator
    emulator64-arm @arm-21 -no-window &
    adb wait-for-device
    adb push $TEST_FILE /data/mio-test
    adb shell /data/mio-test 2>&1 | tee /tmp/out
    grep "^test result.* 0 failed" /tmp/out
    ;;

  aarch64-linux-android)
    # Use the 64bit emulator
    export LD_LIBRARY_PATH="/android/sdk/emulator/lib64/qt/lib:/usr/lib/x86_64-linux-gnu"
    qemu-system-aarch64 @arm64-24 -memory 768 -accel off -gpu off -no-skin -no-window -no-audio -no-snapshot-load -no-snapshot-save &
    adb wait-for-device
    adb root
    adb push $TEST_FILE /data/mio-test
    #adb unroot
    adb shell chmod 755 /data/mio-test
    adb shell /data/mio-test 2>&1 | tee /tmp/out
    grep "^test result.* 0 failed" /tmp/out
    ;;

  *)
    exit 1;
    ;;
esac
