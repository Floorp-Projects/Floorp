#!/bin/bash
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script returns the flags that should be used when GYP_DEFINES contains
# clang_use_chrome_plugins. The flags are stored in a script so that they can
# be changed on the bots without requiring a master restart.

THIS_ABS_DIR=$(cd $(dirname $0) && echo $PWD)
CLANG_LIB_PATH=$THIS_ABS_DIR/../../../third_party/llvm-build/Release+Asserts/lib

if uname -s | grep -q Darwin; then
  LIBSUFFIX=dylib
else
  LIBSUFFIX=so
fi

echo -Xclang -load -Xclang $CLANG_LIB_PATH/libFindBadConstructs.$LIBSUFFIX \
  -Xclang -add-plugin -Xclang find-bad-constructs
