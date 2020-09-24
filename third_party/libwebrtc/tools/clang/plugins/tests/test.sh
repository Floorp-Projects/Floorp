#!/bin/bash
#
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Hacky, primitive testing: This runs the style plugin for a set of input files
# and compares the output with golden result files.

E_BADARGS=65
E_FAILEDTEST=1

failed_any_test=

# Prints usage information.
usage() {
  echo "Usage: $(basename "${0}")" \
    "<Path to the llvm build dir, usually Release+Asserts>"
  echo ""
  echo "  Runs all the libFindBadConstructs unit tests"
  echo ""
}

# Runs a single test case.
do_testcase() {
  local output="$("${CLANG_DIR}"/bin/clang -c -Wno-c++11-extensions \
      -Xclang -load -Xclang "${CLANG_DIR}"/lib/libFindBadConstructs.${LIB} \
      -Xclang -plugin -Xclang find-bad-constructs ${1} 2>&1)"
  local diffout="$(echo "${output}" | diff - "${2}")"
  if [ "${diffout}" = "" ]; then
    echo "PASS: ${1}"
  else
    failed_any_test=yes
    echo "FAIL: ${1}"
    echo "Output of compiler:"
    echo "${output}"
    echo "Expected output:"
    cat "${2}"
    echo
  fi
}

# Validate input to the script.
if [[ -z "${1}" ]]; then
  usage
  exit ${E_BADARGS}
elif [[ ! -d "${1}" ]]; then
  echo "${1} is not a directory."
  usage
  exit ${E_BADARGS}
else
  export CLANG_DIR="${PWD}/${1}"
  echo "Using clang directory ${CLANG_DIR}..."

  # The golden files assume that the cwd is this directory. To make the script
  # work no matter what the cwd is, explicitly cd to there.
  cd "$(dirname "${0}")"

  if [ "$(uname -s)" = "Linux" ]; then
    export LIB=so
  elif [ "$(uname -s)" = "Darwin" ]; then
    export LIB=dylib
  fi
fi

for input in *.cpp; do
  do_testcase "${input}" "${input%cpp}txt"
done

if [[ "${failed_any_test}" ]]; then
  exit ${E_FAILEDTEST}
fi
