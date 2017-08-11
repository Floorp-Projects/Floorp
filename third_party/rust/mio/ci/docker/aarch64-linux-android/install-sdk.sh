#!/bin/sh
# Copyright 2016 The Rust Project Developers. See the COPYRIGHT
# file at the top-level directory of this distribution and at
# http://rust-lang.org/COPYRIGHT.
#
# Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
# http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
# <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
# option. This file may not be copied, modified, or distributed
# except according to those terms.

set -ex

# Prep the SDK and emulator
#
# Note that the update process requires that we accept a bunch of licenses, and
# we can't just pipe `yes` into it for some reason, so we take the same strategy
# located in https://github.com/appunite/docker by just wrapping it in a script
# which apparently magically accepts the licenses.

mkdir sdk

curl -o sdk-tools-linux-3859397.zip https://dl.google.com/android/repository/sdk-tools-linux-3859397.zip && \
    unzip sdk-tools-linux-3859397.zip && \
    mv tools sdk/



yes | sdkmanager --licenses
sdkmanager tools platform-tools "build-tools;25.0.2" "platforms;android-24" "system-images;android-24;default;arm64-v8a"

echo "no" | avdmanager create avd \
                --force \
                --name arm64-24 \
                --package "system-images;android-24;default;arm64-v8a" \
                --abi arm64-v8a \
                --sdcard 256M
