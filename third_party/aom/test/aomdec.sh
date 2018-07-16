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
    if [ ! -e "${AV1_IVF_FILE}" ] || \
       [ ! -e "${AV1_OBU_ANNEXB_FILE}" ] || \
       [ ! -e "${AV1_OBU_SEC5_FILE}" ] || \
       [ ! -e "${AV1_WEBM_FILE}" ]; then
      elog "Libaom test data must exist before running this test script when " \
           " encoding is disabled. "
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
    elog "Input file ($input) missing in aomdec_pipe()"
    return 1
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

aomdec_av1_ivf() {
  if [ "$(aomdec_can_decode_av1)" = "yes" ]; then
    local readonly file="${AV1_IVF_FILE}"
    if [ ! -e "${file}" ]; then
      encode_yuv_raw_input_av1 "${file}" --ivf
    fi
    aomdec "${AV1_IVF_FILE}" --summary --noblit
  fi
}

aomdec_av1_ivf_error_resilient() {
  if [ "$(aomdec_can_decode_av1)" = "yes" ]; then
    local readonly file="av1.error-resilient.ivf"
    if [ ! -e "${file}" ]; then
      encode_yuv_raw_input_av1 "${file}" --ivf --error-resilient=1
    fi
    aomdec "${file}" --summary --noblit
  fi
}

aomdec_av1_ivf_multithread() {
  if [ "$(aomdec_can_decode_av1)" = "yes" ]; then
    local readonly file="${AV1_IVF_FILE}"
    if [ ! -e "${file}" ]; then
      encode_yuv_raw_input_av1 "${file}" --ivf
    fi
    for threads in 2 3 4 5 6 7 8; do
      aomdec "${file}" --summary --noblit --threads=$threads
    done
  fi
}

aomdec_aom_ivf_pipe_input() {
  if [ "$(aomdec_can_decode_av1)" = "yes" ]; then
    local readonly file="${AV1_IVF_FILE}"
    if [ ! -e "${file}" ]; then
      encode_yuv_raw_input_av1 "${file}" --ivf
    fi
    aomdec_pipe "${AV1_IVF_FILE}" --summary --noblit
  fi
}

aomdec_av1_obu_annexb() {
  if [ "$(aomdec_can_decode_av1)" = "yes" ]; then
    local readonly file="${AV1_OBU_ANNEXB_FILE}"
    if [ ! -e "${file}" ]; then
      encode_yuv_raw_input_av1 "${file}" --obu --annexb=1
    fi
    aomdec "${file}" --summary --noblit --annexb
  fi
}

aomdec_av1_obu_section5() {
  if [ "$(aomdec_can_decode_av1)" = "yes" ]; then
    local readonly file="${AV1_OBU_SEC5_FILE}"
    if [ ! -e "${file}" ]; then
      encode_yuv_raw_input_av1 "${file}" --obu
    fi
    aomdec "${file}" --summary --noblit
  fi
}

aomdec_av1_webm() {
  if [ "$(aomdec_can_decode_av1)" = "yes" ] && \
     [ "$(webm_io_available)" = "yes" ]; then
    local readonly file="${AV1_WEBM_FILE}"
    if [ ! -e "${file}" ]; then
      encode_yuv_raw_input_av1 "${file}"
    fi
    aomdec "${AV1_WEBM_FILE}" --summary --noblit
  fi
}

aomdec_tests="aomdec_av1_ivf
              aomdec_av1_ivf_error_resilient
              aomdec_av1_ivf_multithread
              aomdec_aom_ivf_pipe_input
              aomdec_av1_obu_annexb
              aomdec_av1_obu_section5
              aomdec_av1_webm"

run_tests aomdec_verify_environment "${aomdec_tests}"
