#!/bin/bash
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script will check out iwyu into the llvm directory and build it.
# iwyu will appear in third_party/llvm-build/Release+Asserts/bin.

# Die if any command dies.
set -e

# Echo all commands.
set -x

THIS_DIR="$(dirname "${0}")"

# Make sure clang is checked out and built.
"${THIS_DIR}"/update.sh

LLVM_DIR="${THIS_DIR}"/../../../third_party/llvm
IWYU_DIR="${LLVM_DIR}"/tools/clang/tools/include-what-you-use

# Check out.
svn co --force https://include-what-you-use.googlecode.com/svn/trunk/ \
  "${IWYU_DIR}"

# Build iwyu.
# Copy it into the clang tree and use clang's build system to compile it.
LLVM_BUILD_DIR="${LLVM_DIR}"/../llvm-build
IWYU_BUILD_DIR="${LLVM_BUILD_DIR}"/tools/clang/tools/include-what-you-use
mkdir -p "${IWYU_BUILD_DIR}"
cp "${IWYU_DIR}"/Makefile "${IWYU_BUILD_DIR}"
make -j"${NUM_JOBS}" -C "${IWYU_BUILD_DIR}"
