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
##  This file tests the libvpx vp8cx_set_ref example. To add new tests to this
##  file, do the following:
##    1. Write a shell function (this is your test).
##    2. Add the function to vp8cx_set_ref_tests (on a new line).
##
. $(dirname $0)/tools_common.sh

# Environment check: $YUV_RAW_INPUT is required.
vp8cx_set_ref_verify_environment() {
  if [ ! -e "${YUV_RAW_INPUT}" ]; then
    echo "Libvpx test data must exist in LIBVPX_TEST_DATA_PATH."
    return 1
  fi
}

# Runs vp8cx_set_ref and updates the reference frame before encoding frame 90.
# $1 is the codec name, which vp8cx_set_ref does not support at present: It's
# currently used only to name the output file.
# TODO(tomfinegan): Pass the codec param once the example is updated to support
# VP9.
vpx_set_ref() {
  local encoder="${LIBVPX_BIN_PATH}/vp8cx_set_ref${VPX_TEST_EXE_SUFFIX}"
  local codec="$1"
  local output_file="${VPX_TEST_OUTPUT_DIR}/vp8cx_set_ref_${codec}.ivf"
  local ref_frame_num=90

  if [ ! -x "${encoder}" ]; then
    elog "${encoder} does not exist or is not executable."
    return 1
  fi

  eval "${VPX_TEST_PREFIX}" "${encoder}" "${YUV_RAW_INPUT_WIDTH}" \
      "${YUV_RAW_INPUT_HEIGHT}" "${YUV_RAW_INPUT}" "${output_file}" \
      "${ref_frame_num}" ${devnull}

  [ -e "${output_file}" ] || return 1
}

vp8cx_set_ref_vp8() {
  if [ "$(vp8_encode_available)" = "yes" ]; then
    vpx_set_ref vp8 || return 1
  fi
}

vp8cx_set_ref_tests="vp8cx_set_ref_vp8"

run_tests vp8cx_set_ref_verify_environment "${vp8cx_set_ref_tests}"
