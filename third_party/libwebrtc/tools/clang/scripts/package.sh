#!/bin/bash
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script will check out llvm and clang, and then package the results up
# to a tgz file.

THIS_DIR="$(dirname "${0}")"
LLVM_DIR="${THIS_DIR}/../../../third_party/llvm"
LLVM_BOOTSTRAP_DIR="${THIS_DIR}/../../../third_party/llvm-bootstrap"
LLVM_BUILD_DIR="${THIS_DIR}/../../../third_party/llvm-build"
LLVM_BIN_DIR="${LLVM_BUILD_DIR}/Release+Asserts/bin"
LLVM_LIB_DIR="${LLVM_BUILD_DIR}/Release+Asserts/lib"

echo "Diff in llvm:" | tee buildlog.txt
svn stat "${LLVM_DIR}" 2>&1 | tee -a buildlog.txt
svn diff "${LLVM_DIR}" 2>&1 | tee -a buildlog.txt
echo "Diff in llvm/tools/clang:" | tee -a buildlog.txt
svn stat "${LLVM_DIR}/tools/clang" 2>&1 | tee -a buildlog.txt
svn diff "${LLVM_DIR}/tools/clang" 2>&1 | tee -a buildlog.txt
echo "Diff in llvm/projects/compiler-rt:" | tee -a buildlog.txt
svn stat "${LLVM_DIR}/projects/compiler-rt" 2>&1 | tee -a buildlog.txt
svn diff "${LLVM_DIR}/projects/compiler-rt" 2>&1 | tee -a buildlog.txt

echo "Starting build" | tee -a buildlog.txt

set -ex

# Do a clobber build.
rm -rf "${LLVM_BOOTSTRAP_DIR}"
rm -rf "${LLVM_BUILD_DIR}"
"${THIS_DIR}"/update.sh --run-tests --bootstrap --force-local-build 2>&1 | \
    tee -a buildlog.txt

R=$("${LLVM_BIN_DIR}/clang" --version | \
     sed -ne 's/clang version .*(trunk \([0-9]*\))/\1/p')

PDIR=clang-$R
rm -rf $PDIR
mkdir $PDIR
mkdir $PDIR/bin
mkdir $PDIR/lib

# Copy buildlog over.
cp buildlog.txt $PDIR/

# Copy clang into pdir, symlink clang++ to it.
cp "${LLVM_BIN_DIR}/clang" $PDIR/bin/
(cd $PDIR/bin && ln -sf clang clang++ && cd -)

# Copy plugins. Some of the dylibs are pretty big, so copy only the ones we
# care about.
if [ "$(uname -s)" = "Darwin" ]; then
  cp "${LLVM_LIB_DIR}/libFindBadConstructs.dylib" $PDIR/lib
else
  cp "${LLVM_LIB_DIR}/libFindBadConstructs.so" $PDIR/lib
fi

# Copy built-in headers (lib/clang/3.2/include).
# libcompiler-rt puts all kinds of libraries there too, but we want only ASan.
if [ "$(uname -s)" = "Darwin" ]; then
  # Keep only Release+Asserts/lib/clang/3.2/lib/darwin/libclang_rt.asan_osx.a
  find "${LLVM_LIB_DIR}/clang" -type f -path '*lib/darwin*' | grep -v asan | \
       xargs rm
else
  # Keep only
  # Release+Asserts/lib/clang/3.2/lib/linux/libclang_rt.{asan,tsan}-x86_64.a
  # TODO(thakis): Make sure the 32bit version of ASan runtime is kept too once
  # that's built. TSan runtime exists only for 64 bits.
  find "${LLVM_LIB_DIR}/clang" -type f -path '*lib/linux*' | \
       grep -v "asan\|tsan" | xargs rm
fi

cp -R "${LLVM_LIB_DIR}/clang" $PDIR/lib

tar zcf $PDIR.tgz -C $PDIR bin lib buildlog.txt

if [ "$(uname -s)" = "Darwin" ]; then
  PLATFORM=Mac
else
  PLATFORM=Linux_x64
fi

echo To upload, run:
echo gsutil cp -a public-read $PDIR.tgz \
     gs://chromium-browser-clang/$PLATFORM/$PDIR.tgz
