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
##  This file contains shell code shared by test scripts for libaom tools.

# Use $AOM_TEST_TOOLS_COMMON_SH as a pseudo include guard.
if [ -z "${AOM_TEST_TOOLS_COMMON_SH}" ]; then
AOM_TEST_TOOLS_COMMON_SH=included

set -e
devnull='> /dev/null 2>&1'
AOM_TEST_PREFIX=""

elog() {
  echo "$@" 1>&2
}

vlog() {
  if [ "${AOM_TEST_VERBOSE_OUTPUT}" = "yes" ]; then
    echo "$@"
  fi
}

# Sets $AOM_TOOL_TEST to the name specified by positional parameter one.
test_begin() {
  AOM_TOOL_TEST="${1}"
}

# Clears the AOM_TOOL_TEST variable after confirming that $AOM_TOOL_TEST matches
# positional parameter one.
test_end() {
  if [ "$1" != "${AOM_TOOL_TEST}" ]; then
    echo "FAIL completed test mismatch!."
    echo "  completed test: ${1}"
    echo "  active test: ${AOM_TOOL_TEST}."
    return 1
  fi
  AOM_TOOL_TEST='<unset>'
}

# Echoes the target configuration being tested.
test_configuration_target() {
  aom_config_mk="${LIBAOM_CONFIG_PATH}/config.mk"
  # TODO(tomfinegan): Remove the parts requiring config.mk when the configure
  # script is removed from the repository.
  if [ ! -f "${aom_config_mk}" ]; then
    aom_config_c="${LIBAOM_CONFIG_PATH}/aom_config.c"
    # Clean up the cfg pointer line from aom_config.c for easier re-use by
    # someone examining a failure in the example tests.
    # 1. Run grep on aom_config.c for cfg and limit the results to 1.
    # 2. Split the line using ' = ' as separator.
    # 3. Abuse sed to consume the leading " and trailing "; from the assignment
    #    to the cfg pointer.
    cmake_config=$(awk -F ' = ' '/cfg/ { print $NF; exit }' "${aom_config_c}" \
      | sed -e s/\"// -e s/\"\;//)
    echo cmake generated via command: cmake path/to/aom ${cmake_config}
    return
  fi
  # Find the TOOLCHAIN line, split it using ':=' as the field separator, and
  # print the last field to get the value. Then pipe the value to tr to consume
  # any leading/trailing spaces while allowing tr to echo the output to stdout.
  awk -F ':=' '/TOOLCHAIN/ { print $NF }' "${aom_config_mk}" | tr -d ' '
}

# Trap function used for failure reports and tool output directory removal.
# When the contents of $AOM_TOOL_TEST do not match the string '<unset>', reports
# failure of test stored in $AOM_TOOL_TEST.
cleanup() {
  if [ -n "${AOM_TOOL_TEST}" ] && [ "${AOM_TOOL_TEST}" != '<unset>' ]; then
    echo "FAIL: $AOM_TOOL_TEST"
  fi
  if [ -n "${AOM_TEST_OUTPUT_DIR}" ] && [ -d "${AOM_TEST_OUTPUT_DIR}" ]; then
    rm -rf "${AOM_TEST_OUTPUT_DIR}"
  fi
}

# Echoes the git hash portion of the VERSION_STRING variable defined in
# $LIBAOM_CONFIG_PATH/config.mk to stdout, or the version number string when
# no git hash is contained in VERSION_STRING.
config_hash() {
  aom_config_mk="${LIBAOM_CONFIG_PATH}/config.mk"
  if [ ! -f "${aom_config_mk}" ]; then
    aom_config_c="${LIBAOM_CONFIG_PATH}/aom_config.c"
    # Clean up the aom_git_hash pointer line from aom_config.c.
    # 1. Run grep on aom_config.c for aom_git_hash and limit results to 1.
    # 2. Split the line using ' = "' as separator.
    # 3. Abuse sed to consume the trailing "; from the assignment to the
    #    aom_git_hash pointer.
    awk -F ' = "' '/aom_git_hash/ { print $NF; exit }' "${aom_config_c}" \
      | sed s/\"\;//
    return
  fi

  # Find VERSION_STRING line, split it with "-g" and print the last field to
  # output the git hash to stdout.
  aom_version=$(awk -F -g '/VERSION_STRING/ {print $NF}' "${aom_config_mk}")
  # Handle two situations here:
  # 1. The default case: $aom_version is a git hash, so echo it unchanged.
  # 2. When being run a non-dev tree, the -g portion is not present in the
  #    version string: It's only the version number.
  #    In this case $aom_version is something like 'VERSION_STRING=v1.3.0', so
  #    we echo only what is after the '='.
  echo "${aom_version##*=}"
}

# Echoes the short form of the current git hash.
current_hash() {
  if git --version > /dev/null 2>&1; then
    (cd "$(dirname "${0}")"
    git rev-parse HEAD)
  else
    # Return the config hash if git is unavailable: Fail silently, git hashes
    # are used only for warnings.
    config_hash
  fi
}

# Echoes warnings to stdout when git hash in aom_config.h does not match the
# current git hash.
check_git_hashes() {
  hash_at_configure_time=$(config_hash)
  hash_now=$(current_hash)

  if [ "${hash_at_configure_time}" != "${hash_now}" ]; then
    echo "Warning: git hash has changed since last configure."
    vlog "  config hash: ${hash_at_configure_time} hash now: ${hash_now}"
  fi
}

# $1 is the name of an environment variable containing a directory name to
# test.
test_env_var_dir() {
  local dir=$(eval echo "\${$1}")
  if [ ! -d "${dir}" ]; then
    elog "'${dir}': No such directory"
    elog "The $1 environment variable must be set to a valid directory."
    return 1
  fi
}

# This script requires that the LIBAOM_BIN_PATH, LIBAOM_CONFIG_PATH, and
# LIBAOM_TEST_DATA_PATH variables are in the environment: Confirm that
# the variables are set and that they all evaluate to directory paths.
verify_aom_test_environment() {
  test_env_var_dir "LIBAOM_BIN_PATH" \
    && test_env_var_dir "LIBAOM_CONFIG_PATH" \
    && test_env_var_dir "LIBAOM_TEST_DATA_PATH"
}

# Greps aom_config.h in LIBAOM_CONFIG_PATH for positional parameter one, which
# should be a LIBAOM preprocessor flag. Echoes yes to stdout when the feature
# is available.
aom_config_option_enabled() {
  aom_config_option="${1}"
  aom_config_file="${LIBAOM_CONFIG_PATH}/aom_config.h"
  config_line=$(grep "${aom_config_option}" "${aom_config_file}")
  if echo "${config_line}" | egrep -q '1$'; then
    echo yes
  fi
}

# Echoes yes when output of test_configuration_target() contains win32 or win64.
is_windows_target() {
  if test_configuration_target \
     | grep -q -e win32 -e win64 > /dev/null 2>&1; then
    echo yes
  fi
}

# Echoes path to $1 when it's executable and exists in ${LIBAOM_BIN_PATH}, or an
# empty string. Caller is responsible for testing the string once the function
# returns.
aom_tool_path() {
  local readonly tool_name="$1"
  local tool_path="${LIBAOM_BIN_PATH}/${tool_name}${AOM_TEST_EXE_SUFFIX}"
  if [ ! -x "${tool_path}" ]; then
    # Try one directory up: when running via examples.sh the tool could be in
    # the parent directory of $LIBAOM_BIN_PATH.
    tool_path="${LIBAOM_BIN_PATH}/../${tool_name}${AOM_TEST_EXE_SUFFIX}"
  fi

  if [ ! -x "${tool_path}" ]; then
    tool_path=""
  fi
  echo "${tool_path}"
}

# Echoes yes to stdout when the file named by positional parameter one exists
# in LIBAOM_BIN_PATH, and is executable.
aom_tool_available() {
  local tool_name="$1"
  local tool="${LIBAOM_BIN_PATH}/${tool_name}${AOM_TEST_EXE_SUFFIX}"
  [ -x "${tool}" ] && echo yes
}

# Echoes yes to stdout when aom_config_option_enabled() reports yes for
# CONFIG_AV1_DECODER.
av1_decode_available() {
  [ "$(aom_config_option_enabled CONFIG_AV1_DECODER)" = "yes" ] && echo yes
}

# Echoes yes to stdout when aom_config_option_enabled() reports yes for
# CONFIG_AV1_ENCODER.
av1_encode_available() {
  [ "$(aom_config_option_enabled CONFIG_AV1_ENCODER)" = "yes" ] && echo yes
}

# Echoes yes to stdout when aom_config_option_enabled() reports yes for
# CONFIG_WEBM_IO.
webm_io_available() {
  [ "$(aom_config_option_enabled CONFIG_WEBM_IO)" = "yes" ] && echo yes
}

# Filters strings from $1 using the filter specified by $2. Filter behavior
# depends on the presence of $3. When $3 is present, strings that match the
# filter are excluded. When $3 is omitted, strings matching the filter are
# included.
# The filtered result is echoed to stdout.
filter_strings() {
  strings=${1}
  filter=${2}
  exclude=${3}

  if [ -n "${exclude}" ]; then
    # When positional parameter three exists the caller wants to remove strings.
    # Tell grep to invert matches using the -v argument.
    exclude='-v'
  else
    unset exclude
  fi

  if [ -n "${filter}" ]; then
    for s in ${strings}; do
      if echo "${s}" | egrep -q ${exclude} "${filter}" > /dev/null 2>&1; then
        filtered_strings="${filtered_strings} ${s}"
      fi
    done
  else
    filtered_strings="${strings}"
  fi
  echo "${filtered_strings}"
}

# Runs user test functions passed via positional parameters one and two.
# Functions in positional parameter one are treated as environment verification
# functions and are run unconditionally. Functions in positional parameter two
# are run according to the rules specified in aom_test_usage().
run_tests() {
  local env_tests="verify_aom_test_environment $1"
  local tests_to_filter="$2"
  local test_name="${AOM_TEST_NAME}"

  if [ -z "${test_name}" ]; then
    test_name="$(basename "${0%.*}")"
  fi

  if [ "${AOM_TEST_RUN_DISABLED_TESTS}" != "yes" ]; then
    # Filter out DISABLED tests.
    tests_to_filter=$(filter_strings "${tests_to_filter}" ^DISABLED exclude)
  fi

  if [ -n "${AOM_TEST_FILTER}" ]; then
    # Remove tests not matching the user's filter.
    tests_to_filter=$(filter_strings "${tests_to_filter}" ${AOM_TEST_FILTER})
  fi

  # User requested test listing: Dump test names and return.
  if [ "${AOM_TEST_LIST_TESTS}" = "yes" ]; then
    for test_name in $tests_to_filter; do
      echo ${test_name}
    done
    return
  fi

  # Don't bother with the environment tests if everything else was disabled.
  [ -z "${tests_to_filter}" ] && return

  # Combine environment and actual tests.
  local tests_to_run="${env_tests} ${tests_to_filter}"

  check_git_hashes

  # Run tests.
  for test in ${tests_to_run}; do
    test_begin "${test}"
    vlog "  RUN  ${test}"
    "${test}"
    vlog "  PASS ${test}"
    test_end "${test}"
  done

  local tested_config="$(test_configuration_target) @ $(current_hash)"
  echo "${test_name}: Done, all tests pass for ${tested_config}."
}

aom_test_usage() {
cat << EOF
  Usage: ${0##*/} [arguments]
    --bin-path <path to libaom binaries directory>
    --config-path <path to libaom config directory>
    --filter <filter>: User test filter. Only tests matching filter are run.
    --run-disabled-tests: Run disabled tests.
    --help: Display this message and exit.
    --test-data-path <path to libaom test data directory>
    --show-program-output: Shows output from all programs being tested.
    --prefix: Allows for a user specified prefix to be inserted before all test
              programs. Grants the ability, for example, to run test programs
              within valgrind.
    --list-tests: List all test names and exit without actually running tests.
    --verbose: Verbose output.

    When the --bin-path option is not specified the script attempts to use
    \$LIBAOM_BIN_PATH and then the current directory.

    When the --config-path option is not specified the script attempts to use
    \$LIBAOM_CONFIG_PATH and then the current directory.

    When the -test-data-path option is not specified the script attempts to use
    \$LIBAOM_TEST_DATA_PATH and then the current directory.
EOF
}

# Returns non-zero (failure) when required environment variables are empty
# strings.
aom_test_check_environment() {
  if [ -z "${LIBAOM_BIN_PATH}" ] || \
     [ -z "${LIBAOM_CONFIG_PATH}" ] || \
     [ -z "${LIBAOM_TEST_DATA_PATH}" ]; then
    return 1
  fi
}

# Echo aomenc command line parameters allowing use of a raw yuv file as
# input to aomenc.
yuv_raw_input() {
  echo ""${YUV_RAW_INPUT}"
       --width="${YUV_RAW_INPUT_WIDTH}"
       --height="${YUV_RAW_INPUT_HEIGHT}""
}

# Do a small encode for testing decoders.
encode_yuv_raw_input_av1() {
  if [ "$(av1_encode_available)" = "yes" ]; then
    local readonly output="$1"
    local readonly encoder="$(aom_tool_path aomenc)"
    shift
    eval "${encoder}" $(yuv_raw_input) \
      --codec=av1 \
      $@ \
      --limit=5 \
      --output="${output}" \
      ${devnull}

    if [ ! -e "${output}" ]; then
      elog "Output file does not exist."
      return 1
    fi
  fi
}

# Parse the command line.
while [ -n "$1" ]; do
  case "$1" in
    --bin-path)
      LIBAOM_BIN_PATH="$2"
      shift
      ;;
    --config-path)
      LIBAOM_CONFIG_PATH="$2"
      shift
      ;;
    --filter)
      AOM_TEST_FILTER="$2"
      shift
      ;;
    --run-disabled-tests)
      AOM_TEST_RUN_DISABLED_TESTS=yes
      ;;
    --help)
      aom_test_usage
      exit
      ;;
    --test-data-path)
      LIBAOM_TEST_DATA_PATH="$2"
      shift
      ;;
    --prefix)
      AOM_TEST_PREFIX="$2"
      shift
      ;;
    --verbose)
      AOM_TEST_VERBOSE_OUTPUT=yes
      ;;
    --show-program-output)
      devnull=
      ;;
    --list-tests)
      AOM_TEST_LIST_TESTS=yes
      ;;
    *)
      aom_test_usage
      exit 1
      ;;
  esac
  shift
done

# Handle running the tests from a build directory without arguments when running
# the tests on *nix/macosx.
LIBAOM_BIN_PATH="${LIBAOM_BIN_PATH:-.}"
LIBAOM_CONFIG_PATH="${LIBAOM_CONFIG_PATH:-.}"
LIBAOM_TEST_DATA_PATH="${LIBAOM_TEST_DATA_PATH:-.}"

# Create a temporary directory for output files, and a trap to clean it up.
if [ -n "${TMPDIR}" ]; then
  AOM_TEST_TEMP_ROOT="${TMPDIR}"
elif [ -n "${TEMPDIR}" ]; then
  AOM_TEST_TEMP_ROOT="${TEMPDIR}"
else
  AOM_TEST_TEMP_ROOT=/tmp
fi

AOM_TEST_OUTPUT_DIR="${AOM_TEST_TEMP_ROOT}/aom_test_$$"

if ! mkdir -p "${AOM_TEST_OUTPUT_DIR}" || \
   [ ! -d "${AOM_TEST_OUTPUT_DIR}" ]; then
  echo "${0##*/}: Cannot create output directory, giving up."
  echo "${0##*/}:   AOM_TEST_OUTPUT_DIR=${AOM_TEST_OUTPUT_DIR}"
  exit 1
fi

if [ "$(is_windows_target)" = "yes" ]; then
  AOM_TEST_EXE_SUFFIX=".exe"
fi

# Variables shared by tests.
VP8_IVF_FILE="${LIBAOM_TEST_DATA_PATH}/vp80-00-comprehensive-001.ivf"
AV1_IVF_FILE="${LIBAOM_TEST_DATA_PATH}/vp90-2-09-subpixel-00.ivf"

AV1_WEBM_FILE="${LIBAOM_TEST_DATA_PATH}/vp90-2-00-quantizer-00.webm"
AV1_FPM_WEBM_FILE="${LIBAOM_TEST_DATA_PATH}/vp90-2-07-frame_parallel-1.webm"
AV1_LT_50_FRAMES_WEBM_FILE="${LIBAOM_TEST_DATA_PATH}/vp90-2-02-size-32x08.webm"

YUV_RAW_INPUT="${LIBAOM_TEST_DATA_PATH}/hantro_collage_w352h288.yuv"
YUV_RAW_INPUT_WIDTH=352
YUV_RAW_INPUT_HEIGHT=288

Y4M_NOSQ_PAR_INPUT="${LIBAOM_TEST_DATA_PATH}/park_joy_90p_8_420_a10-1.y4m"
Y4M_720P_INPUT="${LIBAOM_TEST_DATA_PATH}/niklas_1280_720_30.y4m"

# Setup a trap function to clean up after tests complete.
trap cleanup EXIT

vlog "$(basename "${0%.*}") test configuration:
  LIBAOM_BIN_PATH=${LIBAOM_BIN_PATH}
  LIBAOM_CONFIG_PATH=${LIBAOM_CONFIG_PATH}
  LIBAOM_TEST_DATA_PATH=${LIBAOM_TEST_DATA_PATH}
  AOM_IVF_FILE=${AOM_IVF_FILE}
  AV1_IVF_FILE=${AV1_IVF_FILE}
  AV1_WEBM_FILE=${AV1_WEBM_FILE}
  AOM_TEST_EXE_SUFFIX=${AOM_TEST_EXE_SUFFIX}
  AOM_TEST_FILTER=${AOM_TEST_FILTER}
  AOM_TEST_LIST_TESTS=${AOM_TEST_LIST_TESTS}
  AOM_TEST_OUTPUT_DIR=${AOM_TEST_OUTPUT_DIR}
  AOM_TEST_PREFIX=${AOM_TEST_PREFIX}
  AOM_TEST_RUN_DISABLED_TESTS=${AOM_TEST_RUN_DISABLED_TESTS}
  AOM_TEST_SHOW_PROGRAM_OUTPUT=${AOM_TEST_SHOW_PROGRAM_OUTPUT}
  AOM_TEST_TEMP_ROOT=${AOM_TEST_TEMP_ROOT}
  AOM_TEST_VERBOSE_OUTPUT=${AOM_TEST_VERBOSE_OUTPUT}
  YUV_RAW_INPUT=${YUV_RAW_INPUT}
  YUV_RAW_INPUT_WIDTH=${YUV_RAW_INPUT_WIDTH}
  YUV_RAW_INPUT_HEIGHT=${YUV_RAW_INPUT_HEIGHT}
  Y4M_NOSQ_PAR_INPUT=${Y4M_NOSQ_PAR_INPUT}"

fi  # End $AOM_TEST_TOOLS_COMMON_SH pseudo include guard.
