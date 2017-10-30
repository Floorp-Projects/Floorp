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
curl https://dl.google.com/android/repository/tools_r25.2.5-linux.zip -O
unzip -d sdk tools_r25.2.5-linux.zip

filter="platform-tools,android-24"

case "$1" in
  arm | armv7)
    abi=armeabi-v7a
    ;;

  aarch64)
    abi=arm64-v8a
    ;;

  i686)
    abi=x86
    ;;

  x86_64)
    abi=x86_64
    ;;

  *)
    echo "invalid arch: $1"
    exit 1
    ;;
esac;

filter="$filter,sys-img-$abi-android-24"

./android-accept-licenses.sh "android - update sdk -a --no-ui --filter $filter"

echo "no" | android create avd \
                --name $1 \
                --target android-24 \
                --abi $abi
