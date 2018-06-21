#!/bin/sh

# Copyright 2018 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

main() {
  export PATH="$HOME/depot_tools:$PATH"
  ./gyptest.py -a -f ninja
}

main "$@"
