#!/bin/bash

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A wrapper that runs the program and filters the output through\
# asan_symbolize.py and c++filt
#
# TODO(glider): this should be removed once EmbeddedTool in valgrind_test.py
# starts supporting pipes.

export THISDIR=`dirname $0`
"$@" 2>&1 |
  $THISDIR/../../../third_party/asan/scripts/asan_symbolize.py |
  c++filt
