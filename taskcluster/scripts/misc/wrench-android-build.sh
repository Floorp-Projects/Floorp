#!/bin/bash
set -x -e -v

MODE=${1?"First argument must be debug|release"}

pushd "${MOZ_FETCHES_DIR}"
mv wrench-deps/{vendor,.cargo} ${GECKO_PATH}/gfx/wr
popd

pushd "${GECKO_PATH}/gfx/wr/wrench"
# These things come from the toolchain dependencies of the job that invokes
# this script (webrender-wrench-android-build).
export PATH="${PATH}:${MOZ_FETCHES_DIR}/rustc/bin"
export PATH="${PATH}:${JAVA_HOME}/bin"
export ANDROID_SDK_ROOT="${MOZ_FETCHES_DIR}/android-sdk-linux"
export ANDROID_NDK_ROOT="${MOZ_FETCHES_DIR}/android-ndk"

if [ "$MODE" == "debug" ]; then
    $MOZ_FETCHES_DIR/cargo-apk/cargo-apk apk build --frozen --verbose --lib
elif [ "$MODE" == "release" ]; then
    $MOZ_FETCHES_DIR/cargo-apk/cargo-apk apk build --frozen --verbose --lib --release
else
    echo "Unknown mode '${MODE}'; must be 'debug' or 'release'"
    exit 1
fi
popd
