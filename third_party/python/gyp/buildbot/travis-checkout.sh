#!/bin/sh

# Copyright 2018 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -ex

get_depot_tools() {
  cd
  git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
  export PATH="$HOME/depot_tools:$PATH"
}

gclient_sync() {
  cd "${TRAVIS_BUILD_DIR}"/..
  gclient config --unmanaged https://github.com/chromium/gyp.git
  gclient sync
  cd gyp
}

main() {
  get_depot_tools
  gclient_sync
}

main "$@"
