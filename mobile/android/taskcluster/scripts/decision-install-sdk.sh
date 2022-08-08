#!/bin/bash

set -ex

export CURL='curl --location --retry 5'

ANDROID_SDK_VERSION='3859397'
ANDROID_SDK_SHA256='444e22ce8ca0f67353bda4b85175ed3731cae3ffa695ca18119cbacef1c1bea0'
SDK_ZIP_LOCATION="$HOME/sdk-tools-linux.zip"

# For the Android build system we want Java 11. However this version of sdkmanager requires Java 8.
JAVA8PATH="/usr/lib/jvm/java-8-openjdk-amd64/jre/bin/:$PATH"

$CURL --output "$SDK_ZIP_LOCATION" "https://dl.google.com/android/repository/sdk-tools-linux-${ANDROID_SDK_VERSION}.zip"
echo "$ANDROID_SDK_SHA256  $SDK_ZIP_LOCATION" | sha256sum --check
unzip -d "$ANDROID_SDK_ROOT" "$SDK_ZIP_LOCATION"
rm "$SDK_ZIP_LOCATION"

yes | PATH=$JAVA8PATH "${ANDROID_SDK_ROOT}/tools/bin/sdkmanager" --licenses
