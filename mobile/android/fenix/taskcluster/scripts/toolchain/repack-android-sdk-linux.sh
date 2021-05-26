#!/bin/bash

export ANDROID_SDK_ROOT=$MOZ_FETCHES_DIR

# For the Android build system we want Java 11. However this version of sdkmanager requires Java 8.
JAVA8PATH="/usr/lib/jvm/java-8-openjdk-amd64/jre/bin/:$PATH"

yes | PATH=$JAVA8PATH "${ANDROID_SDK_ROOT}/tools/bin/sdkmanager" --licenses

# It's nice to have the build logs include the state of the world upon completion.
PATH=$JAVA8PATH "${ANDROID_SDK_ROOT}/tools/bin/sdkmanager" --list

tar cf - -C "$ANDROID_SDK_ROOT" . --transform 's,^\./,android-sdk-linux/,' | xz > "$UPLOAD_DIR/android-sdk-linux.tar.xz"
