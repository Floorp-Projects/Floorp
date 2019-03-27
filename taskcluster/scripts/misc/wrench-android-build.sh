#!/bin/bash
set -x -e -v

pushd "${GECKO_PATH}"
./mach artifact toolchain -v $MOZ_TOOLCHAINS
mv wrench-deps/{vendor,.cargo,cargo-apk} gfx/wr
popd

pushd "${GECKO_PATH}/gfx/wr/wrench"
# These things come from the toolchain dependencies of the job that invokes
# this script (webrender-wrench-android-build).
export PATH="${PATH}:${GECKO_PATH}/rustc/bin:${GECKO_PATH}/android-gradle-dependencies/gradle-dist/bin"
export PATH="${PATH}:${GECKO_PATH}/rustc/bin"
export ANDROID_HOME="${GECKO_PATH}/android-sdk-linux"
export NDK_HOME="${GECKO_PATH}/android-ndk"
../cargo-apk/bin/cargo-apk build --frozen --verbose
popd
