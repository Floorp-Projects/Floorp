#!/usr/bin/env bash
# Copyright (c) the JPEG XL Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

# Continuous integration helper module. This module is meant to be called from
# the .gitlab-ci.yml file during the continuous integration build, as well as
# from the command line for developers.

# This wrapper is used to enable WASM SIMD when running tests.
# Unfortunately, it is impossible to pass the option directly via the
# CMAKE_CROSSCOMPILING_EMULATOR variable.

# Fallback to default v8 binary, if override is not set.
V8="${V8:-$(which v8)}"
SCRIPT="$1"
shift
"${V8}" --experimental-wasm-simd "${SCRIPT}" -- "$@"
