#!/bin/bash

set -ex

export CURL='curl --location --retry 5'

ANDROID_SDK_VERSION='3859397'
ANDROID_SDK_SHA256='444e22ce8ca0f67353bda4b85175ed3731cae3ffa695ca18119cbacef1c1bea0'
SDK_ZIP_LOCATION="$HOME/sdk-tools-linux.zip"

$CURL --output "$SDK_ZIP_LOCATION" "https://dl.google.com/android/repository/sdk-tools-linux-${ANDROID_SDK_VERSION}.zip"
echo "$ANDROID_SDK_SHA256  $SDK_ZIP_LOCATION" | sha256sum --check
unzip -d "$ANDROID_SDK_ROOT" "$SDK_ZIP_LOCATION"
rm "$SDK_ZIP_LOCATION"

export JAVA_HOME="/usr/lib/jvm/java-1.8.0-openjdk-amd64"
yes | "${ANDROID_SDK_ROOT}/tools/bin/sdkmanager" --licenses
