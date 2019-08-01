#!/bin/bash
set -x -e -v

MODE=${1?"First argument must be debug|release"}

pushd "${MOZ_FETCHES_DIR}"
mv wrench-deps/{vendor,.cargo,cargo-apk} ${GECKO_PATH}/gfx/wr
popd

pushd "${GECKO_PATH}/gfx/wr/wrench"
# The following maven links are equivalent to GRADLE_MAVEN_REPOSITORIES, try
# and keep in sync
cat > build.gradle.inc <<END
buildscript {
    repositories {
        maven{ url uri('file:${MOZ_FETCHES_DIR}/android-gradle-dependencies/google') }
        maven{ url uri('file:${MOZ_FETCHES_DIR}/android-gradle-dependencies/jcenter') }
    }
    dependencies {
        classpath 'com.android.tools.build:gradle:3.4.2'
    }
}
allprojects {
    repositories {
        maven{ url uri('file:${MOZ_FETCHES_DIR}/android-gradle-dependencies/google') }
        maven{ url uri('file:${MOZ_FETCHES_DIR}/android-gradle-dependencies/jcenter') }
    }
}
END
# These things come from the toolchain dependencies of the job that invokes
# this script (webrender-wrench-android-build).
export PATH="${PATH}:${MOZ_FETCHES_DIR}/rustc/bin"
export ANDROID_HOME="${MOZ_FETCHES_DIR}/android-sdk-linux"
export NDK_HOME="${MOZ_FETCHES_DIR}/android-ndk"
export CARGO_APK_GRADLE_COMMAND="${MOZ_FETCHES_DIR}/android-gradle-dependencies/gradle-dist/bin/gradle"
export CARGO_APK_BUILD_GRADLE_INC="${PWD}/build.gradle.inc"

if [ "$MODE" == "debug" ]; then
    ../cargo-apk/bin/cargo-apk build --frozen --verbose
elif [ "$MODE" == "release" ]; then
    ../cargo-apk/bin/cargo-apk build --frozen --verbose --release
    ${GECKO_PATH}/mobile/android/debug_sign_tool.py --verbose \
        ../target/android-artifacts/app/build/outputs/apk/release/app-release-unsigned.apk
else
    echo "Unknown mode '${MODE}'; must be 'debug' or 'release'"
    exit 1
fi
popd
