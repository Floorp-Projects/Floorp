#!/bin/bash -vex

set -x -e

echo "running as" $(id)

set -v

cd $GECKO_PATH

# Nexus needs Java 8
export PATH=$MOZ_FETCHES_DIR/jdk-8/bin:$PATH

. taskcluster/scripts/misc/android-gradle-dependencies/before.sh

export MOZCONFIG=mobile/android/config/mozconfigs/android-arm-gradle-dependencies/nightly
./mach build
./mach gradle downloadDependencies
./mach android gradle-dependencies
pushd mobile/android/fenix
./gradlew detekt lint assembleDebug mozilla-lint-rules:test
popd
pushd mobile/android/focus-android
./gradlew lint
popd
pushd mobile/android/android-components
# Before building anything we explicitly build one component that contains Glean and initializes
# the Miniconda Python environment and doesn't have (almost) any other transitive dependencies.
# If that happens concurrently with other tasks then this seems to fail quite often.
./gradlew service-nimbus:build
./gradlew -Pcoverage detekt lint service-nimbus:assembleAndroidTest samples-browser:testGeckoDebugUnitTest tooling-lint:test
popd

. taskcluster/scripts/misc/android-gradle-dependencies/after.sh
