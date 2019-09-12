#!/bin/bash

set +x

ANDROID_SDK_VERSION=3859397

curl -o "$HOME/sdk-tools-linux.zip" "https://dl.google.com/android/repository/sdk-tools-linux-${ANDROID_SDK_VERSION}.zip"
unzip -d "$ANDROID_SDK_ROOT" "$HOME/sdk-tools-linux.zip"
yes | "${ANDROID_SDK_ROOT}/tools/bin/sdkmanager" --licenses
