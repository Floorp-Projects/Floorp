#!/bin/sh
##
##  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##
##
## This script generates 'VPX.framework'. An iOS app can encode and decode VPx
## video by including 'VPX.framework'.
##
## Run iosbuild.sh to create 'VPX.framework' in the current directory.
##
set -e
devnull='> /dev/null 2>&1'

BUILD_ROOT="_iosbuild"
CONFIGURE_ARGS="--disable-docs
                --disable-examples
                --disable-libyuv
                --disable-unit-tests"
DIST_DIR="_dist"
FRAMEWORK_DIR="VPX.framework"
HEADER_DIR="${FRAMEWORK_DIR}/Headers/vpx"
MAKE_JOBS=1
SCRIPT_DIR=$(dirname "$0")
LIBVPX_SOURCE_DIR=$(cd ${SCRIPT_DIR}/../..; pwd)
LIPO=$(xcrun -sdk iphoneos${SDK} -find lipo)
ORIG_PWD="$(pwd)"
TARGETS="arm64-darwin-gcc
         armv7-darwin-gcc
         armv7s-darwin-gcc
         x86-iphonesimulator-gcc
         x86_64-iphonesimulator-gcc"

# Configures for the target specified by $1, and invokes make with the dist
# target using $DIST_DIR as the distribution output directory.
build_target() {
  local target="$1"
  local old_pwd="$(pwd)"

  vlog "***Building target: ${target}***"

  mkdir "${target}"
  cd "${target}"
  eval "${LIBVPX_SOURCE_DIR}/configure" --target="${target}" \
    ${CONFIGURE_ARGS} ${EXTRA_CONFIGURE_ARGS} ${devnull}
  export DIST_DIR
  eval make -j ${MAKE_JOBS} dist ${devnull}
  cd "${old_pwd}"

  vlog "***Done building target: ${target}***"
}

# Returns the preprocessor symbol for the target specified by $1.
target_to_preproc_symbol() {
  target="$1"
  case "${target}" in
    arm64-*)
      echo "__aarch64__"
      ;;
    armv7-*)
      echo "__ARM_ARCH_7A__"
      ;;
    armv7s-*)
      echo "__ARM_ARCH_7S__"
      ;;
    x86-*)
      echo "__i386__"
      ;;
    x86_64-*)
      echo "__x86_64__"
      ;;
    *)
      echo "#error ${target} unknown/unsupported"
      return 1
      ;;
  esac
}

# Create a vpx_config.h shim that, based on preprocessor settings for the
# current target CPU, includes the real vpx_config.h for the current target.
# $1 is the list of targets.
create_vpx_framework_config_shim() {
  local targets="$1"
  local config_file="${HEADER_DIR}/vpx_config.h"
  local preproc_symbol=""
  local target=""
  local include_guard="VPX_FRAMEWORK_HEADERS_VPX_VPX_CONFIG_H_"

  local file_header="/*
 *  Copyright (c) $(date +%Y) The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/* GENERATED FILE: DO NOT EDIT! */

#ifndef ${include_guard}
#define ${include_guard}

#if defined"

  printf "%s" "${file_header}" > "${config_file}"
  for target in ${targets}; do
    preproc_symbol=$(target_to_preproc_symbol "${target}")
    printf " ${preproc_symbol}\n" >> "${config_file}"
    printf "#define VPX_FRAMEWORK_TARGET \"${target}\"\n" >> "${config_file}"
    printf "#include \"VPX/vpx/${target}/vpx_config.h\"\n" >> "${config_file}"
    printf "#elif defined" >> "${config_file}"
    mkdir "${HEADER_DIR}/${target}"
    cp -p "${BUILD_ROOT}/${target}/vpx_config.h" "${HEADER_DIR}/${target}"
  done

  # Consume the last line of output from the loop: We don't want it.
  sed -i '' -e '$d' "${config_file}"

  printf "#endif\n\n" >> "${config_file}"
  printf "#endif  // ${include_guard}" >> "${config_file}"
}

# Configures and builds each target specified by $1, and then builds
# VPX.framework.
build_framework() {
  local lib_list=""
  local targets="$1"
  local target=""
  local target_dist_dir=""

  # Clean up from previous build(s).
  rm -rf "${BUILD_ROOT}" "${FRAMEWORK_DIR}"

  # Create output dirs.
  mkdir -p "${BUILD_ROOT}"
  mkdir -p "${HEADER_DIR}"

  cd "${BUILD_ROOT}"

  for target in ${targets}; do
    build_target "${target}"
    target_dist_dir="${BUILD_ROOT}/${target}/${DIST_DIR}"
    lib_list="${lib_list} ${target_dist_dir}/lib/libvpx.a"
  done

  cd "${ORIG_PWD}"

  # The basic libvpx API includes are all the same; just grab the most recent
  # set.
  cp -p "${target_dist_dir}"/include/vpx/* "${HEADER_DIR}"

  # Build the fat library.
  ${LIPO} -create ${lib_list} -output ${FRAMEWORK_DIR}/VPX

  # Create the vpx_config.h shim that allows usage of vpx_config.h from
  # within VPX.framework.
  create_vpx_framework_config_shim "${targets}"

  # Copy in vpx_version.h.
  cp -p "${BUILD_ROOT}/${target}/vpx_version.h" "${HEADER_DIR}"

  vlog "Created fat library ${FRAMEWORK_DIR}/VPX containing:"
  for lib in ${lib_list}; do
    vlog "  $(echo ${lib} | awk -F / '{print $2, $NF}')"
  done

  # TODO(tomfinegan): Verify that expected targets are included within
  # VPX.framework/VPX via lipo -info.
}

# Trap function. Cleans up the subtree used to build all targets contained in
# $TARGETS.
cleanup() {
  local readonly res=$?
  cd "${ORIG_PWD}"

  if [ $res -ne 0 ]; then
    elog "build exited with error ($res)"
  fi

  if [ "${PRESERVE_BUILD_OUTPUT}" != "yes" ]; then
    rm -rf "${BUILD_ROOT}"
  fi
}

iosbuild_usage() {
cat << EOF
  Usage: ${0##*/} [arguments]
    --help: Display this message and exit.
    --extra-configure-args <args>: Extra args to pass when configuring libvpx.
    --jobs: Number of make jobs.
    --preserve-build-output: Do not delete the build directory.
    --show-build-output: Show output from each library build.
    --targets <targets>: Override default target list. Defaults:
         ${TARGETS}
    --verbose: Output information about the environment and each stage of the
               build.
EOF
}

elog() {
  echo "${0##*/} failed because: $@" 1>&2
}

vlog() {
  if [ "${VERBOSE}" = "yes" ]; then
    echo "$@"
  fi
}

trap cleanup EXIT

# Parse the command line.
while [ -n "$1" ]; do
  case "$1" in
    --extra-configure-args)
      EXTRA_CONFIGURE_ARGS="$2"
      shift
      ;;
    --help)
      iosbuild_usage
      exit
      ;;
    --jobs)
      MAKE_JOBS="$2"
      shift
      ;;
    --preserve-build-output)
      PRESERVE_BUILD_OUTPUT=yes
      ;;
    --show-build-output)
      devnull=
      ;;
    --targets)
      TARGETS="$2"
      shift
      ;;
    --verbose)
      VERBOSE=yes
      ;;
    *)
      iosbuild_usage
      exit 1
      ;;
  esac
  shift
done

if [ "${VERBOSE}" = "yes" ]; then
cat << EOF
  BUILD_ROOT=${BUILD_ROOT}
  DIST_DIR=${DIST_DIR}
  CONFIGURE_ARGS=${CONFIGURE_ARGS}
  EXTRA_CONFIGURE_ARGS=${EXTRA_CONFIGURE_ARGS}
  FRAMEWORK_DIR=${FRAMEWORK_DIR}
  HEADER_DIR=${HEADER_DIR}
  MAKE_JOBS=${MAKE_JOBS}
  PRESERVE_BUILD_OUTPUT=${PRESERVE_BUILD_OUTPUT}
  LIBVPX_SOURCE_DIR=${LIBVPX_SOURCE_DIR}
  LIPO=${LIPO}
  ORIG_PWD=${ORIG_PWD}
  TARGETS="${TARGETS}"
EOF
fi

build_framework "${TARGETS}"
echo "Successfully built '${FRAMEWORK_DIR}' for:"
echo "         ${TARGETS}"
