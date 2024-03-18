#!/bin/bash

export ANDROID_SDK_ROOT=$MOZ_FETCHES_DIR

JAVA17PATH="/usr/lib/jvm/java-17-openjdk-amd64/bin/:$PATH"

yes | PATH=$JAVA17PATH "${ANDROID_SDK_ROOT}/cmdline-tools/bin/sdkmanager" --licenses --sdk_root="${ANDROID_SDK_ROOT}"

# It's nice to have the build logs include the state of the world upon completion.
PATH=$JAVA17PATH "${ANDROID_SDK_ROOT}/cmdline-tools/bin/sdkmanager" --list --sdk_root="${ANDROID_SDK_ROOT}"

tar cf - -C "$ANDROID_SDK_ROOT" . --transform 's,^\./,android-sdk-linux/,' | xz > "$UPLOAD_DIR/android-sdk-linux.tar.xz"
