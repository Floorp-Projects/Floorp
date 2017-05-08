#!/bin/sh
## Copyright (c) 2016, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
## This file tests aomdec. To add new tests to this file, do the following:
##   1. Write a shell function (this is your test).
##   2. Add the function to aomdec_tests (on a new line).
##
. $(dirname $0)/tools_common.sh

# Environment check: Make sure input is available.
aomdec_verify_environment() {
  if [ "$(av1_encode_available)" != "yes" ] ; then
    if [ ! -e "${AV1_WEBM_FILE}" ] || \
      [ ! -e "${AV1_FPM_WEBM_FILE}" ] || \
      [ ! -e "${AV1_LT_50_FRAMES_WEBM_FILE}" ] ; then
      elog "Libaom test data must exist in LIBAOM_TEST_DATA_PATH."
      return 1
    fi
  fi
  if [ -z "$(aom_tool_path aomdec)" ]; then
    elog "aomdec not found. It must exist in LIBAOM_BIN_PATH or its parent."
    return 1
  fi
}

# Wrapper function for running aomdec with pipe input. Requires that
# LIBAOM_BIN_PATH points to the directory containing aomdec. $1 is used as the
# input file path and shifted away. All remaining parameters are passed through
# to aomdec.
aomdec_pipe() {
  local readonly input="$1"
  shift
  if [ ! -e "${input}" ]; then
    local file="${AOM_TEST_OUTPUT_DIR}/test_encode.ivf"
    encode_yuv_raw_input_av1 "${file}" --ivf
  else
    local file="${input}"
  fi
  cat "${file}" | aomdec - "$@" ${devnull}
}


# Wrapper function for running aomdec. Requires that LIBAOM_BIN_PATH points to
# the directory containing aomdec. $1 one is used as the input file path and
# shifted away. All remaining parameters are passed through to aomdec.
aomdec() {
  local readonly decoder="$(aom_tool_path aomdec)"
  local readonly input="$1"
  shift
  eval "${AOM_TEST_PREFIX}" "${decoder}" "$input" "$@" ${devnull}
}

aomdec_can_decode_av1() {
  if [ "$(av1_decode_available)" = "yes" ]; then
    echo yes
  fi
}

aomdec_aom_ivf_pipe_input() {
  if [ "$(aomdec_can_decode_av1)" = "yes" ]; then
    aomdec_pipe "${AOM_IVF_FILE}" --summary --noblit
  fi
}

aomdec_av1_webm() {
  if [ "$(aomdec_can_decode_av1)" = "yes" ] && \
     [ "$(webm_io_available)" = "yes" ]; then
    if [ ! -e "${AV1_WEBM_FILE}" ]; then
      local file="${AOM_TEST_OUTPUT_DIR}/test_encode.webm"
      encode_yuv_raw_input_av1 "${file}"
    else
      aomdec "${AV1_WEBM_FILE}" --summary --noblit
    fi
  fi
}

aomdec_av1_webm_frame_parallel() {
  if [ "$(aomdec_can_decode_av1)" = "yes" ] && \
     [ "$(webm_io_available)" = "yes" ]; then
    local file
    if [ ! -e "${AV1_WEBM_FILE}" ]; then
      file="${AOM_TEST_OUTPUT_DIR}/test_encode.webm"
      encode_yuv_raw_input_av1 "${file}" "--ivf --error-resilient=1 "
    else
      file="${AV1_FPM_WEBM_FILE}"
    fi
    for threads in 2 3 4 5 6 7 8; do
      aomdec "${file}" --summary --noblit --threads=$threads \
        --frame-parallel
    done
  fi
}

# TODO(vigneshv): Enable or remove this test and associated code.
DISABLED_aomdec_av1_webm_less_than_50_frames() {
  # ensure that reaching eof in webm_guess_framerate doesn't result in invalid
  # frames in actual webm_read_frame calls.
  if [ "$(aomdec_can_decode_av1)" = "yes" ] && \
     [ "$(webm_io_available)" = "yes" ]; then
    local readonly decoder="$(aom_tool_path aomdec)"
    local readonly expected=10
    local readonly num_frames=$(${AOM_TEST_PREFIX} "${decoder}" \
      "${AV1_LT_50_FRAMES_WEBM_FILE}" --summary --noblit 2>&1 \
      | awk '/^[0-9]+ decoded frames/ { print $1 }')
    if [ "$num_frames" -ne "$expected" ]; then
      elog "Output frames ($num_frames) != expected ($expected)"
      return 1
    fi
  fi
}

aomdec_tests="aomdec_av1_webm
              aomdec_av1_webm_frame_parallel
              aomdec_aom_ivf_pipe_input
              DISABLED_aomdec_av1_webm_less_than_50_frames"

run_tests aomdec_verify_environment "${aomdec_tests}"
