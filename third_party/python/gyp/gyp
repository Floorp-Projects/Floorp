#!/bin/sh
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -eu
base=$(dirname "$0")
if type python3 >& /dev/null; then
  python=python3
else
  python=python
fi
exec "${python}" "${base}/gyp_main.py" "$@"
