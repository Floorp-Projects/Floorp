#!/bin/bash

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

# The script is to install Android SDK, NDK for build chromium on Android, and
# doesn't need to run as root.

# Using Android 4.0, API Level: 14 (ice cream sandwich). The SDK package is
# about 25M.
SDK_FILE_NAME="android-sdk_r16-linux.tgz"
SDK_DOWNLOAD_URL="http://dl.google.com/android/${SDK_FILE_NAME}"
SDK_MD5SUM="3ba457f731d51da3741c29c8830a4583"

# Using "ANDROID_SDK_ROOT/tools/android list targets" to get the matching target
# id which will be loaded in simulator for testing.
# For example: the output of the listed the target could be below, and the
# 'android-13' is the SDK_TARGET_ID in this case.
# id: 9 or "android-13"
#     Name: Android 3.2
#     Type: Platform
#     API level: 13
#     Revision: 1
#     Skins: WXGA (default)
SDK_TARGET_ID=android-14

# Using NDK r7; The package is about 64M.
NDK_FILE_NAME="android-ndk-r7-linux-x86.tar.bz2"
NDK_DOWNLOAD_URL="http://dl.google.com/android/ndk/${NDK_FILE_NAME}"
NDK_MD5SUM="bf15e6b47bf50824c4b96849bf003ca3"

# The temporary directory used to store the downloaded file.
TEMPDIR=$(mktemp -d)
cleanup() {
  local status=${?}
  trap - EXIT
  rm -rf "${TEMPDIR}"
  exit ${status}
}
trap cleanup EXIT

##########################################################
# Download and install a tgz package by wget and tar -xvf.
# The current directory is changed in this function.
# Arguments:
#   local_file_name, the name of downloaded file.
#   download_url, the url to download the package.
#   md5, the package's md5 which could be found in download page.
#   install_path, where the package should be installed.
# Returns:
#   None
##########################################################
install_dev_kit() {
  local local_file_name="${1}"
  local download_url="${2}"
  local md5="${3}"
  local install_path="${4}"

  cd "${TEMPDIR}"
  wget "${download_url}"

  local computed_md5=$(md5sum "${local_file_name}" | cut -d' ' -f1)
  if [[ "${computed_md5}" != "${md5}" ]]; then
    echo "Downloaded ${local_file_name} has bad md5sum, which is expected" >& 2
    echo "to be ${md5} but was ${computed_md5}" >& 2
    exit 1
  fi

  echo "Install ${local_file_name}"
  mv "${local_file_name}" "${install_path}"
  cd "${install_path}"
  tar -xvf "${local_file_name}"
}

if [[ -z "${ANDROID_SDK_ROOT}" ]]; then
  echo "Please set ANDROID_SDK_ROOT to where they should installed to." >& 2
  echo "For example: /usr/local/android-sdk-linux_x86" >& 2
  exit 1
fi

if [[ -z "${ANDROID_NDK_ROOT}" ]]; then
  echo "Please set ANDROID_NDK_ROOT to where they should installed to." >& 2
  echo "For example: /usr/local/android-ndk-r6b" >& 2
  exit 1
fi

# Install Android SDK if it doesn't exist.
if [[ ! -d "${ANDROID_SDK_ROOT}" ]]; then
  echo 'Install ANDROID SDK ...'
  (install_dev_kit "${SDK_FILE_NAME}" "${SDK_DOWNLOAD_URL}" "${SDK_MD5SUM}" \
                  $(dirname "${ANDROID_SDK_ROOT}"))
fi

# Install the target if it doesn't exist. The package installed above contains
# no platform, platform-tool or tool, all those should be installed by
# ${ANDROID_SDK_ROOT}/tools/android.
if [[ ! $("${ANDROID_SDK_ROOT}/tools/android" list targets \
  | grep -q "${SDK_TARGET_ID}") ]]; then
  # Updates the SDK by installing the necessary components.
  # From current configuration, all android platforms will be installed.
  # This will take a little bit long time.
  echo "Install platform, platform-tool and tool ..."

  "${ANDROID_SDK_ROOT}"/tools/android update sdk -o --no-ui \
      --filter platform,platform-tool,tool,system-image
fi

# Create a Android Virtual Device named 'buildbot' with default hardware
# configuration and override the existing one, since there is no easy way to
# check whether current AVD has correct configuration and it takes almost no
# time to create a new one.
"${ANDROID_SDK_ROOT}/tools/android" --silent create avd --name buildbot \
  --target ${SDK_TARGET_ID} --force <<< "no"

# Install Android NDK if it doesn't exist.
if [[ ! -d "${ANDROID_NDK_ROOT}" ]]; then
  echo 'Install ANDROID NDK ...'
  (install_dev_kit "${NDK_FILE_NAME}" "${NDK_DOWNLOAD_URL}" "${NDK_MD5SUM}" \
                  $(dirname "${ANDROID_NDK_ROOT}"))
fi
