#!/bin/bash
set -x -e -v

MODE=${1?"First argument must be debug|release"}

pushd "${MOZ_FETCHES_DIR}"
mv wrench-deps/{vendor,.cargo,cargo-apk} ${GECKO_PATH}/gfx/wr
popd

pushd "${GECKO_PATH}/gfx/wr/wrench"
# These things come from the toolchain dependencies of the job that invokes
# this script (webrender-wrench-android-build).
export PATH="${PATH}:${MOZ_FETCHES_DIR}/rustc/bin"
export ANDROID_HOME="${MOZ_FETCHES_DIR}/android-sdk-linux"
export NDK_HOME="${MOZ_FETCHES_DIR}/android-ndk"

# `cargo apk build` fails whilst attempting to sign the output apk,
# unless the `.android` directory exists.
# See https://github.com/rust-windowing/android-rs-glue/issues/252
mkdir /builds/worker/.android
if [ "$MODE" == "debug" ]; then
    ../cargo-apk/bin/cargo-apk build --frozen --verbose
elif [ "$MODE" == "release" ]; then
    ../cargo-apk/bin/cargo-apk build --frozen --verbose --release
else
    echo "Unknown mode '${MODE}'; must be 'debug' or 'release'"
    exit 1
fi
popd
