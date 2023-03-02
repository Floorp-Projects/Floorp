#!/bin/bash

export ANDROID_SDK_ROOT=$MOZ_FETCHES_DIR

JAVA11PATH="/usr/lib/jvm/java-11-openjdk-amd64/bin/:$PATH"

yes | PATH=$JAVA11PATH "${ANDROID_SDK_ROOT}/cmdline-tools/bin/sdkmanager" --licenses --sdk_root="${ANDROID_SDK_ROOT}"

# It's nice to have the build logs include the state of the world upon completion.
PATH=$JAVA11PATH "${ANDROID_SDK_ROOT}/cmdline-tools/bin/sdkmanager" --list --sdk_root="${ANDROID_SDK_ROOT}"

tar cf - -C "$ANDROID_SDK_ROOT" . --transform 's,^\./,android-sdk-linux/,' | xz > "$UPLOAD_DIR/android-sdk-linux.tar.xz"
