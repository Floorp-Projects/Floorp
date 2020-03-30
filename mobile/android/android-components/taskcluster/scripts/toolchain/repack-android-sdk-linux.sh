#!/bin/bash

export ANDROID_SDK_ROOT=$MOZ_FETCHES_DIR

yes | "${ANDROID_SDK_ROOT}/tools/bin/sdkmanager" --licenses

# It's nice to have the build logs include the state of the world upon completion.
"${ANDROID_SDK_ROOT}/tools/bin/sdkmanager" --list

tar cf - -C "$ANDROID_SDK_ROOT" . --transform 's,^\./,android-sdk-linux/,' | xz > "$UPLOAD_DIR/android-sdk-linux.tar.xz"
