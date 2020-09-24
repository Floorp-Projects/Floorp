#!/usr/bin/env bash
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script will check out llvm and clang into third_party/llvm and build it.

# Do NOT CHANGE this if you don't know what you're doing -- see
# https://code.google.com/p/chromium/wiki/UpdatingClang
# Reverting problematic clang rolls is safe, though.
CLANG_REVISION=163674

THIS_DIR="$(dirname "${0}")"
LLVM_DIR="${THIS_DIR}/../../../third_party/llvm"
LLVM_BUILD_DIR="${LLVM_DIR}/../llvm-build"
LLVM_BOOTSTRAP_DIR="${LLVM_DIR}/../llvm-bootstrap"
CLANG_DIR="${LLVM_DIR}/tools/clang"
COMPILER_RT_DIR="${LLVM_DIR}/projects/compiler-rt"
STAMP_FILE="${LLVM_BUILD_DIR}/cr_build_revision"

# ${A:-a} returns $A if it's set, a else.
LLVM_REPO_URL=${LLVM_URL:-https://llvm.org/svn/llvm-project}

# Die if any command dies.
set -e

OS="$(uname -s)"

# Parse command line options.
force_local_build=
mac_only=
run_tests=
bootstrap=
while [[ $# > 0 ]]; do
  case $1 in
    --bootstrap)
      bootstrap=yes
      ;;
    --force-local-build)
      force_local_build=yes
      ;;
    --mac-only)
      mac_only=yes
      ;;
    --run-tests)
      run_tests=yes
      ;;
    --help)
      echo "usage: $0 [--force-local-build] [--mac-only] [--run-tests] "
      echo "--bootstrap: First build clang with CC, then with itself."
      echo "--force-local-build: Don't try to download prebuilt binaries."
      echo "--mac-only: Do initial download only on Mac systems."
      echo "--run-tests: Run tests after building. Only for local builds."
      exit 1
      ;;
  esac
  shift
done

# --mac-only prevents the initial download on non-mac systems, but if clang has
# already been downloaded in the past, this script keeps it up to date even if
# --mac-only is passed in and the system isn't a mac. People who don't like this
# can just delete their third_party/llvm-build directory.
if [[ -n "$mac_only" ]] && [[ "${OS}" != "Darwin" ]] &&
    [[ "$GYP_DEFINES" != *clang=1* ]] && ! [[ -d "${LLVM_BUILD_DIR}" ]]; then
  exit 0
fi

# Xcode and clang don't get along when predictive compilation is enabled.
# http://crbug.com/96315
if [[ "${OS}" = "Darwin" ]] && xcodebuild -version | grep -q 'Xcode 3.2' ; then
  XCONF=com.apple.Xcode
  if [[ "${GYP_GENERATORS}" != "make" ]] && \
     [ "$(defaults read "${XCONF}" EnablePredictiveCompilation)" != "0" ]; then
    echo
    echo "          HEARKEN!"
    echo "You're using Xcode3 and you have 'Predictive Compilation' enabled."
    echo "This does not work well with clang (http://crbug.com/96315)."
    echo "Disable it in Preferences->Building (lower right), or run"
    echo "    defaults write ${XCONF} EnablePredictiveCompilation -boolean NO"
    echo "while Xcode is not running."
    echo
  fi

  SUB_VERSION=$(xcodebuild -version | sed -Ene 's/Xcode 3\.2\.([0-9]+)/\1/p')
  if [[ "${SUB_VERSION}" < 6 ]]; then
    echo
    echo "          YOUR LD IS BUGGY!"
    echo "Please upgrade Xcode to at least 3.2.6."
    echo
  fi
fi


# Check if there's anything to be done, exit early if not.
if [[ -f "${STAMP_FILE}" ]]; then
  PREVIOUSLY_BUILT_REVISON=$(cat "${STAMP_FILE}")
  if [[ -z "$force_local_build" ]] && \
       [[ "${PREVIOUSLY_BUILT_REVISON}" = "${CLANG_REVISION}" ]]; then
    echo "Clang already at ${CLANG_REVISION}"
    exit 0
  fi
fi
# To always force a new build if someone interrupts their build half way.
rm -f "${STAMP_FILE}"

# Clobber pch files, since they only work with the compiler version that
# created them. Also clobber .o files, to make sure everything will be built
# with the new compiler.
if [[ "${OS}" = "Darwin" ]]; then
  XCODEBUILD_DIR="${THIS_DIR}/../../../xcodebuild"

  # Xcode groups .o files by project first, configuration second.
  if [[ -d "${XCODEBUILD_DIR}" ]]; then
    echo "Clobbering .o files for Xcode build"
    find "${XCODEBUILD_DIR}" -name '*.o' -exec rm {} +
  fi
fi

if [ -f "${THIS_DIR}/../../../WebKit.gyp" ]; then
  # We're inside a WebKit checkout.
  # TODO(thakis): try to unify the directory layout of the xcode- and
  # make-based builds. http://crbug.com/110455
  MAKE_DIR="${THIS_DIR}/../../../../../../out"
else
  # We're inside a Chromium checkout.
  MAKE_DIR="${THIS_DIR}/../../../out"
fi

for CONFIG in Debug Release; do
  if [[ -d "${MAKE_DIR}/${CONFIG}/obj.target" ||
        -d "${MAKE_DIR}/${CONFIG}/obj.host" ]]; then
    echo "Clobbering ${CONFIG} PCH and .o files for make build"
    if [[ -d "${MAKE_DIR}/${CONFIG}/obj.target" ]]; then
      find "${MAKE_DIR}/${CONFIG}/obj.target" -name '*.gch' -exec rm {} +
      find "${MAKE_DIR}/${CONFIG}/obj.target" -name '*.o' -exec rm {} +
    fi
    if [[ -d "${MAKE_DIR}/${CONFIG}/obj.host" ]]; then
      find "${MAKE_DIR}/${CONFIG}/obj.host" -name '*.o' -exec rm {} +
    fi
  fi

  # ninja puts its output below ${MAKE_DIR} as well.
  if [[ -d "${MAKE_DIR}/${CONFIG}/obj" ]]; then
    echo "Clobbering ${CONFIG} PCH and .o files for ninja build"
    find "${MAKE_DIR}/${CONFIG}/obj" -name '*.gch' -exec rm {} +
    find "${MAKE_DIR}/${CONFIG}/obj" -name '*.o' -exec rm {} +
    find "${MAKE_DIR}/${CONFIG}/obj" -name '*.o.d' -exec rm {} +
  fi

  if [[ "${OS}" = "Darwin" ]]; then
    if [[ -d "${XCODEBUILD_DIR}/${CONFIG}/SharedPrecompiledHeaders" ]]; then
      echo "Clobbering ${CONFIG} PCH files for Xcode build"
      rm -rf "${XCODEBUILD_DIR}/${CONFIG}/SharedPrecompiledHeaders"
    fi
  fi
done

if [[ -z "$force_local_build" ]]; then
  # Check if there's a prebuilt binary and if so just fetch that. That's faster,
  # and goma relies on having matching binary hashes on client and server too.
  CDS_URL=https://commondatastorage.googleapis.com/chromium-browser-clang
  CDS_FILE="clang-${CLANG_REVISION}.tgz"
  CDS_OUT_DIR=$(mktemp -d -t clang_download.XXXXXX)
  CDS_OUTPUT="${CDS_OUT_DIR}/${CDS_FILE}"
  if [ "${OS}" = "Linux" ]; then
    CDS_FULL_URL="${CDS_URL}/Linux_x64/${CDS_FILE}"
  elif [ "${OS}" = "Darwin" ]; then
    CDS_FULL_URL="${CDS_URL}/Mac/${CDS_FILE}"
  fi
  echo Trying to download prebuilt clang
  if which curl > /dev/null; then
    curl -L --fail "${CDS_FULL_URL}" -o "${CDS_OUTPUT}" || \
        rm -rf "${CDS_OUT_DIR}"
  elif which wget > /dev/null; then
    wget "${CDS_FULL_URL}" -O "${CDS_OUTPUT}" || rm -rf "${CDS_OUT_DIR}"
  else
    echo "Neither curl nor wget found. Please install one of these."
    exit 1
  fi
  if [ -f "${CDS_OUTPUT}" ]; then
    rm -rf "${LLVM_BUILD_DIR}/Release+Asserts"
    mkdir -p "${LLVM_BUILD_DIR}/Release+Asserts"
    tar -xzf "${CDS_OUTPUT}" -C "${LLVM_BUILD_DIR}/Release+Asserts"
    echo clang "${CLANG_REVISION}" unpacked
    echo "${CLANG_REVISION}" > "${STAMP_FILE}"
    rm -rf "${CDS_OUT_DIR}"
    exit 0
  else
    echo Did not find prebuilt clang at r"${CLANG_REVISION}", building
  fi
fi

echo Getting LLVM r"${CLANG_REVISION}" in "${LLVM_DIR}"
if ! svn co --force "${LLVM_REPO_URL}/llvm/trunk@${CLANG_REVISION}" \
                    "${LLVM_DIR}"; then
  echo Checkout failed, retrying
  rm -rf "${LLVM_DIR}"
  svn co --force "${LLVM_REPO_URL}/llvm/trunk@${CLANG_REVISION}" "${LLVM_DIR}"
fi

echo Getting clang r"${CLANG_REVISION}" in "${CLANG_DIR}"
svn co --force "${LLVM_REPO_URL}/cfe/trunk@${CLANG_REVISION}" "${CLANG_DIR}"

echo Getting compiler-rt r"${CLANG_REVISION}" in "${COMPILER_RT_DIR}"
svn co --force "${LLVM_REPO_URL}/compiler-rt/trunk@${CLANG_REVISION}" \
               "${COMPILER_RT_DIR}"

# Echo all commands.
set -x

NUM_JOBS=3
if [[ "${OS}" = "Linux" ]]; then
  NUM_JOBS="$(grep -c "^processor" /proc/cpuinfo)"
elif [ "${OS}" = "Darwin" ]; then
  NUM_JOBS="$(sysctl -n hw.ncpu)"
fi

# Build bootstrap clang if requested.
if [[ -n "${bootstrap}" ]]; then
  echo "Building bootstrap compiler"
  mkdir -p "${LLVM_BOOTSTRAP_DIR}"
  cd "${LLVM_BOOTSTRAP_DIR}"
  if [[ ! -f ./config.status ]]; then
    # The bootstrap compiler only needs to be able to build the real compiler,
    # so it needs no cross-compiler output support. In general, the host
    # compiler should be as similar to the final compiler as possible, so do
    # keep --disable-threads & co.
    ../llvm/configure \
        --enable-optimized \
        --enable-targets=host-only \
        --disable-threads \
        --disable-pthreads \
        --without-llvmgcc \
        --without-llvmgxx
    MACOSX_DEPLOYMENT_TARGET=10.5 make -j"${NUM_JOBS}"
  fi
  if [[ -n "${run_tests}" ]]; then
    make check-all
  fi
  cd -
  export CC="${PWD}/${LLVM_BOOTSTRAP_DIR}/Release+Asserts/bin/clang"
  export CXX="${PWD}/${LLVM_BOOTSTRAP_DIR}/Release+Asserts/bin/clang++"
  echo "Building final compiler"
fi

# Build clang (in a separate directory).
# The clang bots have this path hardcoded in built/scripts/slave/compile.py,
# so if you change it you also need to change these links.
mkdir -p "${LLVM_BUILD_DIR}"
cd "${LLVM_BUILD_DIR}"
if [[ ! -f ./config.status ]]; then
  ../llvm/configure \
      --enable-optimized \
      --disable-threads \
      --disable-pthreads \
      --without-llvmgcc \
      --without-llvmgxx
fi

MACOSX_DEPLOYMENT_TARGET=10.5 make -j"${NUM_JOBS}"
cd -

# Build plugin.
# Copy it into the clang tree and use clang's build system to compile the
# plugin.
PLUGIN_SRC_DIR="${THIS_DIR}/../plugins"
PLUGIN_DST_DIR="${LLVM_DIR}/tools/clang/tools/chrome-plugin"
PLUGIN_BUILD_DIR="${LLVM_BUILD_DIR}/tools/clang/tools/chrome-plugin"
rm -rf "${PLUGIN_DST_DIR}"
cp -R "${PLUGIN_SRC_DIR}" "${PLUGIN_DST_DIR}"
rm -rf "${PLUGIN_BUILD_DIR}"
mkdir -p "${PLUGIN_BUILD_DIR}"
cp "${PLUGIN_SRC_DIR}/Makefile" "${PLUGIN_BUILD_DIR}"
MACOSX_DEPLOYMENT_TARGET=10.5 make -j"${NUM_JOBS}" -C "${PLUGIN_BUILD_DIR}"

if [[ -n "$run_tests" ]]; then
  # Run a few tests.
  "${PLUGIN_SRC_DIR}/tests/test.sh" "${LLVM_BUILD_DIR}/Release+Asserts"
  cd "${LLVM_BUILD_DIR}"
  make check-all
  cd -
fi

# After everything is done, log success for this revision.
echo "${CLANG_REVISION}" > "${STAMP_FILE}"
