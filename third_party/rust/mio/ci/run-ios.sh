#!/bin/sh

# Builds and runs tests for a particular target passed as an argument to this
# script.

set -ex

TARGET=$1


case "$TARGET" in
  *-apple-ios)
    # Download the iOS test harness
    curl -vv -L https://github.com/carllerche/ios-test-harness/releases/download/v0.1.0/libiosharness-$TARGET.a > libiosharness.a;

    # Build the test
    cargo rustc --test test --target $TARGET -- \
        -L . \
        -C link-args="-mios-simulator-version-min=7.0 -e _ios_main -liosharness";


    # Find the file to run
    TEST_FILE="$(find target/$TARGET/debug -maxdepth 1 -type f -name test-* | head -1)";

    rustc -O ./ci/ios/deploy_and_run_on_ios_simulator.rs;
    ./deploy_and_run_on_ios_simulator $TEST_FILE;

    ;;

  *)
    echo "unsupported target $TARGET";
    exit 1;
    ;;
esac
