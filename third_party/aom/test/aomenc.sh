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
## This file tests aomenc using hantro_collage_w352h288.yuv as input. To add
## new tests to this file, do the following:
##   1. Write a shell function (this is your test).
##   2. Add the function to aomenc_tests (on a new line).
##
. $(dirname $0)/tools_common.sh

readonly TEST_FRAMES=5

# Environment check: Make sure input is available.
aomenc_verify_environment() {
  if [ ! -e "${YUV_RAW_INPUT}" ]; then
    elog "The file ${YUV_RAW_INPUT##*/} must exist in LIBAOM_TEST_DATA_PATH."
    return 1
  fi
  if [ "$(aomenc_can_encode_av1)" = "yes" ]; then
    if [ ! -e "${Y4M_NOSQ_PAR_INPUT}" ]; then
      elog "The file ${Y4M_NOSQ_PAR_INPUT##*/} must exist in"
      elog "LIBAOM_TEST_DATA_PATH."
      return 1
    fi
  fi
  if [ -z "$(aom_tool_path aomenc)" ]; then
    elog "aomenc not found. It must exist in LIBAOM_BIN_PATH or its parent."
    return 1
  fi
}

aomenc_can_encode_av1() {
  if [ "$(av1_encode_available)" = "yes" ]; then
    echo yes
  fi
}

aomenc_can_encode_av1() {
  if [ "$(av1_encode_available)" = "yes" ]; then
    echo yes
  fi
}

# Utilities that echo aomenc input file parameters.
y4m_input_non_square_par() {
  echo ""${Y4M_NOSQ_PAR_INPUT}""
}

y4m_input_720p() {
  echo ""${Y4M_720P_INPUT}""
}

# Echo default aomenc real time encoding params. $1 is the codec, which defaults
# to av1 if unspecified.
aomenc_rt_params() {
  local readonly codec="${1:-av1}"
  echo "--codec=${codec}
    --buf-initial-sz=500
    --buf-optimal-sz=600
    --buf-sz=1000
    --cpu-used=-6
    --end-usage=cbr
    --error-resilient=1
    --kf-max-dist=90000
    --lag-in-frames=0
    --max-intra-rate=300
    --max-q=56
    --min-q=2
    --noise-sensitivity=0
    --overshoot-pct=50
    --passes=1
    --profile=0
    --resize-allowed=0
    --rt
    --static-thresh=0
    --undershoot-pct=50"
}

# Wrapper function for running aomenc with pipe input. Requires that
# LIBAOM_BIN_PATH points to the directory containing aomenc. $1 is used as the
# input file path and shifted away. All remaining parameters are passed through
# to aomenc.
aomenc_pipe() {
  local readonly encoder="$(aom_tool_path aomenc)"
  local readonly input="$1"
  shift
  cat "${input}" | eval "${AOM_TEST_PREFIX}" "${encoder}" - \
    --test-decode=fatal \
    "$@" ${devnull}
}

# Wrapper function for running aomenc. Requires that LIBAOM_BIN_PATH points to
# the directory containing aomenc. $1 one is used as the input file path and
# shifted away. All remaining parameters are passed through to aomenc.
aomenc() {
  local readonly encoder="$(aom_tool_path aomenc)"
  local readonly input="$1"
  shift
  eval "${AOM_TEST_PREFIX}" "${encoder}" "${input}" \
    --test-decode=fatal \
    "$@" ${devnull}
}

aomenc_av1_ivf() {
  if [ "$(aomenc_can_encode_av1)" = "yes" ]; then
    local readonly output="${AOM_TEST_OUTPUT_DIR}/av1.ivf"
    aomenc $(yuv_raw_input) \
      --codec=av1 \
      --limit="${TEST_FRAMES}" \
      --ivf \
      --output="${output}"

    if [ ! -e "${output}" ]; then
      elog "Output file does not exist."
      return 1
    fi
  fi
}

aomenc_av1_webm() {
  if [ "$(aomenc_can_encode_av1)" = "yes" ] && \
     [ "$(webm_io_available)" = "yes" ]; then
    local readonly output="${AOM_TEST_OUTPUT_DIR}/av1.webm"
    aomenc $(yuv_raw_input) \
      --codec=av1 \
      --limit="${TEST_FRAMES}" \
      --output="${output}"

    if [ ! -e "${output}" ]; then
      elog "Output file does not exist."
      return 1
    fi
  fi
}

aomenc_av1_webm_2pass() {
  if [ "$(aomenc_can_encode_av1)" = "yes" ] && \
     [ "$(webm_io_available)" = "yes" ]; then
    local readonly output="${AOM_TEST_OUTPUT_DIR}/av1.webm"
    aomenc $(yuv_raw_input) \
      --codec=av1 \
      --limit="${TEST_FRAMES}" \
      --output="${output}" \
      --passes=2

    if [ ! -e "${output}" ]; then
      elog "Output file does not exist."
      return 1
    fi
  fi
}

aomenc_av1_ivf_lossless() {
  if [ "$(aomenc_can_encode_av1)" = "yes" ]; then
    local readonly output="${AOM_TEST_OUTPUT_DIR}/av1_lossless.ivf"
    aomenc $(yuv_raw_input) \
      --codec=av1 \
      --limit="${TEST_FRAMES}" \
      --ivf \
      --output="${output}" \
      --lossless=1

    if [ ! -e "${output}" ]; then
      elog "Output file does not exist."
      return 1
    fi
  fi
}

aomenc_av1_ivf_minq0_maxq0() {
  if [ "$(aomenc_can_encode_av1)" = "yes" ]; then
    local readonly output="${AOM_TEST_OUTPUT_DIR}/av1_lossless_minq0_maxq0.ivf"
    aomenc $(yuv_raw_input) \
      --codec=av1 \
      --limit="${TEST_FRAMES}" \
      --ivf \
      --output="${output}" \
      --min-q=0 \
      --max-q=0

    if [ ! -e "${output}" ]; then
      elog "Output file does not exist."
      return 1
    fi
  fi
}

aomenc_av1_webm_lag5_frames10() {
  if [ "$(aomenc_can_encode_av1)" = "yes" ] && \
     [ "$(webm_io_available)" = "yes" ]; then
    local readonly lag_total_frames=10
    local readonly lag_frames=5
    local readonly output="${AOM_TEST_OUTPUT_DIR}/av1_lag5_frames10.webm"
    aomenc $(yuv_raw_input) \
      --codec=av1 \
      --limit="${lag_total_frames}" \
      --lag-in-frames="${lag_frames}" \
      --output="${output}" \
      --passes=2 \
      --auto-alt-ref=1

    if [ ! -e "${output}" ]; then
      elog "Output file does not exist."
      return 1
    fi
  fi
}

# TODO(fgalligan): Test that DisplayWidth is different than video width.
aomenc_av1_webm_non_square_par() {
  if [ "$(aomenc_can_encode_av1)" = "yes" ] && \
     [ "$(webm_io_available)" = "yes" ]; then
    local readonly output="${AOM_TEST_OUTPUT_DIR}/av1_non_square_par.webm"
    aomenc $(y4m_input_non_square_par) \
      --codec=av1 \
      --limit="${TEST_FRAMES}" \
      --output="${output}"

    if [ ! -e "${output}" ]; then
      elog "Output file does not exist."
      return 1
    fi
  fi
}

aomenc_tests="aomenc_av1_ivf
              aomenc_av1_webm
              aomenc_av1_webm_2pass
              aomenc_av1_ivf_lossless
              aomenc_av1_ivf_minq0_maxq0
              aomenc_av1_webm_lag5_frames10
              aomenc_av1_webm_non_square_par"

run_tests aomenc_verify_environment "${aomenc_tests}"
