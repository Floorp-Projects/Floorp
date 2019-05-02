#!/bin/bash
set -x -e -v

MODE=${1?"First argument must be debug|release"}

pushd "${GECKO_PATH}"
./mach artifact toolchain -v $MOZ_TOOLCHAINS
mv wrench-deps/{vendor,.cargo,cargo-apk} gfx/wr
popd

pushd "${GECKO_PATH}/gfx/wr/wrench"
# The following maven links are equivalent to GRADLE_MAVEN_REPOSITORIES, try
# and keep in sync
cat > build.gradle.inc <<END
buildscript {
    repositories {
        maven{ url uri('file:${GECKO_PATH}/android-gradle-dependencies/google') }
        maven{ url uri('file:${GECKO_PATH}/android-gradle-dependencies/jcenter') }
    }
    dependencies {
        classpath 'com.android.tools.build:gradle:3.1.4'
    }
}
allprojects {
    repositories {
        maven{ url uri('file:${GECKO_PATH}/android-gradle-dependencies/google') }
        maven{ url uri('file:${GECKO_PATH}/android-gradle-dependencies/jcenter') }
    }
}
END
# These things come from the toolchain dependencies of the job that invokes
# this script (webrender-wrench-android-build).
export PATH="${PATH}:${GECKO_PATH}/rustc/bin"
export ANDROID_HOME="${GECKO_PATH}/android-sdk-linux"
export NDK_HOME="${GECKO_PATH}/android-ndk"
export CARGO_APK_GRADLE_COMMAND="${GECKO_PATH}/android-gradle-dependencies/gradle-dist/bin/gradle"
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
