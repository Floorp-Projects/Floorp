#!/bin/bash
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Bash functions used by buildbot annotator scripts for the android
# build of chromium.  Executing this script should not perform actions
# other than setting variables and defining of functions.

# Number of jobs on the compile line; e.g.  make -j"${JOBS}"
JOBS="${JOBS:-4}"

# Clobber build?  Overridden by bots with BUILDBOT_CLOBBER.
NEED_CLOBBER="${NEED_CLOBBER:-0}"


# Parse named arguments passed into the annotator script
# and assign them global variable names.
function bb_parse_args {
  while [[ $1 ]]; do
    case "$1" in
      --factory-properties=*)
        FACTORY_PROPERTIES="$(echo "$1" | sed 's/^[^=]*=//')"
        ;;
      --build-properties=*)
        BUILD_PROPERTIES="$(echo "$1" | sed 's/^[^=]*=//')"
        ;;
      *)
        echo "@@@STEP_WARNINGS@@@"
        echo "Warning, unparsed input argument: '$1'"
        ;;
    esac
    shift
  done
}

# Function to force-green a bot.
function bb_force_bot_green_and_exit {
  echo "@@@BUILD_STEP Bot forced green.@@@"
  exit 0
}

function bb_run_gclient_hooks {
  echo "@@@BUILD_STEP runhooks android@@@"
  gclient runhooks
}

# Basic setup for all bots to run after a source tree checkout.
# Args:
#   $1: source root.
#   $2 and beyond: key value pairs which are parsed by bb_parse_args.
function bb_baseline_setup {
  echo "@@@BUILD_STEP Environment setup@@@"
  SRC_ROOT="$1"
  # Remove SRC_ROOT param
  shift

  bb_parse_args "$@"

  if [ ! -d "${SRC_ROOT}" ] ; then
    echo "Please specify a valid source root directory as an arg"
    echo '@@@STEP_FAILURE@@@'
    return 1
  fi
  cd $SRC_ROOT

  if [ ! -f build/android/envsetup.sh ] ; then
    echo "No envsetup.sh"
    echo "@@@STEP_FAILURE@@@"
    return 1
  fi

  for mandatory_directory in $(dirname "${ANDROID_SDK_ROOT}") \
    $(dirname "${ANDROID_NDK_ROOT}") ; do
    if [[ ! -d "${mandatory_directory}" ]]; then
      echo "Directory ${mandatory_directory} does not exist."
      echo "Build cannot continue."
      echo "@@@STEP_FAILURE@@@"
      return 1
    fi
  done

  if [ ! "$BUILDBOT_CLOBBER" = "" ]; then
    NEED_CLOBBER=1
  fi

  . build/android/envsetup.sh

  if [ "$NEED_CLOBBER" -eq 1 ]; then
    echo "@@@BUILD_STEP Clobber@@@"
    rm -rf "${SRC_ROOT}"/out
    if [ -e "${SRC_ROOT}"/out ] ; then
      echo "Clobber appeared to fail?  ${SRC_ROOT}/out still exists."
      echo "@@@STEP_WARNINGS@@@"
    fi
  fi

  # Should be called only after envsetup is done.
  bb_run_gclient_hooks
}


# Setup goma.  Used internally to buildbot_functions.sh.
function bb_setup_goma_internal {

  # Quick bail if I messed things up and can't wait for the CQ to
  # flush out.
  # TODO(jrg): remove this condition when things are
  # proven stable (4/1/12 or so).
  if [ -f /usr/local/google/DISABLE_GOMA ]; then
    echo "@@@STEP_WARNINGS@@@"
    echo "Goma disabled with a local file"
    return
  fi

  goma_dir=${goma_dir:-/b/build/goma}
  if [ -f ${goma_dir}/goma.key ]; then
    export GOMA_API_KEY_FILE=${GOMA_DIR}/goma.key
  fi
  local goma_ctl=$(which goma_ctl.sh)
  if [ "${goma_ctl}" != "" ]; then
    local goma_dir=$(dirname ${goma_ctl})
  fi

  if [ ! -f ${goma_dir}/goma_ctl.sh ]; then
    echo "@@@STEP_WARNINGS@@@"
    echo "Goma not found on this machine; defaulting to make"
    return
  fi
  export GOMA_DIR=${goma_dir}
  echo "GOMA_DIR: " $GOMA_DIR

  export GOMA_COMPILER_PROXY_DAEMON_MODE=true
  export GOMA_COMPILER_PROXY_RPC_TIMEOUT_SECS=300
  export PATH=$GOMA_DIR:$PATH

  echo "Starting goma"
  if [ "$NEED_CLOBBER" -eq 1 ]; then
    ${GOMA_DIR}/goma_ctl.sh restart
  else
    ${GOMA_DIR}/goma_ctl.sh ensure_start
  fi
  trap bb_stop_goma_internal SIGHUP SIGINT SIGTERM
}

# Stop goma.
function bb_stop_goma_internal {
  echo "Stopping goma"
  ${GOMA_DIR}/goma_ctl.sh stop
}

# $@: make args.
# Use goma if possible; degrades to non-Goma if needed.
function bb_goma_make {
  bb_setup_goma_internal

  if [ "${GOMA_DIR}" = "" ]; then
    make -j${JOBS} "$@"
    return
  fi

  HOST_CC=$GOMA_DIR/gcc
  HOST_CXX=$GOMA_DIR/g++
  TARGET_CC=$(/bin/ls $ANDROID_TOOLCHAIN/*-gcc | head -n1)
  TARGET_CXX=$(/bin/ls $ANDROID_TOOLCHAIN/*-g++ | head -n1)
  TARGET_CC="$GOMA_DIR/gomacc $TARGET_CC"
  TARGET_CXX="$GOMA_DIR/gomacc $TARGET_CXX"
  COMMON_JAVAC="$GOMA_DIR/gomacc /usr/bin/javac -J-Xmx512M \
    -target 1.5 -Xmaxerrs 9999999"

  command make \
    -j100 \
    -l20 \
    HOST_CC="$HOST_CC" \
    HOST_CXX="$HOST_CXX" \
    TARGET_CC="$TARGET_CC" \
    TARGET_CXX="$TARGET_CXX" \
    CC.host="$HOST_CC" \
    CXX.host="$HOST_CXX" \
    CC.target="$TARGET_CC" \
    CXX.target="$TARGET_CXX" \
    LINK.target="$TARGET_CXX" \
    COMMON_JAVAC="$COMMON_JAVAC" \
    "$@"

  local make_exit_code=$?
  bb_stop_goma_internal
  return $make_exit_code
}

# Compile step
function bb_compile {
  # This must be named 'compile', not 'Compile', for CQ interaction.
  # Talk to maruel for details.
  echo "@@@BUILD_STEP compile@@@"
  bb_goma_make
}

# Experimental compile step; does not turn the tree red if it fails.
function bb_compile_experimental {
  # Linking DumpRenderTree appears to hang forever?
  EXPERIMENTAL_TARGETS="android_experimental"
  for target in ${EXPERIMENTAL_TARGETS} ; do
    echo "@@@BUILD_STEP Experimental Compile $target @@@"
    set +e
    bb_goma_make -k "${target}"
    if [ $? -ne 0 ] ; then
      echo "@@@STEP_WARNINGS@@@"
    fi
    set -e
  done
}

# Run tests on an emulator.
function bb_run_tests_emulator {
  echo "@@@BUILD_STEP Run Tests on an Emulator@@@"
  build/android/run_tests.py -e --xvfb --verbose
}

# Run tests on an actual device.  (Better have one plugged in!)
function bb_run_tests {
  python build/android/device_status_check.py
  echo "@@@BUILD_STEP Run Tests on actual hardware@@@"
  build/android/run_tests.py --xvfb --verbose
}

# Run instrumentation test.
# Args:
#   $1: TEST_APK.
#   $2: EXTRA_FLAGS to be passed to run_instrumentation_tests.py.
function bb_run_instrumentation_test {
  local TEST_APK=${1}
  local EXTRA_FLAGS=${2}
  local APK_NAME=$(basename ${TEST_APK})
  echo "@@@BUILD_STEP Android Instrumentation ${APK_NAME} ${EXTRA_FLAGS} "\
       "on actual hardware@@@"
  local INSTRUMENTATION_FLAGS="-vvv"
  INSTRUMENTATION_FLAGS+=" --test-apk ${TEST_APK}"
  INSTRUMENTATION_FLAGS+=" ${EXTRA_FLAGS}"
  build/android/run_instrumentation_tests.py ${INSTRUMENTATION_FLAGS}
}

# Run content shell instrumentation test on device.
function bb_run_content_shell_instrumentation_test {
  build/android/adb_install_content_shell
  local TEST_APK="content_shell_test/ContentShellTest-debug"
  # Use -I to install the test apk only on the first run.
  # TODO(bulach): remove the second once we have a Smoke test.
  bb_run_instrumentation_test ${TEST_APK} "-I -A Smoke"
  bb_run_instrumentation_test ${TEST_APK} "-I -A SmallTest"
  bb_run_instrumentation_test ${TEST_APK} "-A MediumTest"
  bb_run_instrumentation_test ${TEST_APK} "-A LargeTest"
}

# Zip and archive a build.
function bb_zip_build {
  echo "@@@BUILD_STEP Zip build@@@"
  python ../../../../scripts/slave/zip_build.py \
    --src-dir "$SRC_ROOT" \
    --exclude-files "lib.target" \
    --factory-properties "$FACTORY_PROPERTIES" \
    --build-properties "$BUILD_PROPERTIES"
}

# Download and extract a build.
function bb_extract_build {
  echo "@@@BUILD_STEP Download and extract build@@@"
  if [[ -z $FACTORY_PROPERTIES || -z $BUILD_PROPERTIES ]]; then
    return 1
  fi

  # When extract_build.py downloads an unversioned build it
  # issues a warning by exiting with large numbered return code
  # When it fails to download it build, it exits with return
  # code 1.  We disable halt on error mode and return normally
  # unless the python tool returns 1.
  (
  set +e
  python ../../../../scripts/slave/extract_build.py \
    --build-dir "$SRC_ROOT" \
    --build-output-dir "out" \
    --factory-properties "$FACTORY_PROPERTIES" \
    --build-properties "$BUILD_PROPERTIES"
  local extract_exit_code=$?
  if (( $extract_exit_code > 1 )); then
    echo "@@@STEP_WARNINGS@@@"
    return
  fi
  return $extract_exit_code
  )
}

# Reboot all phones and wait for them to start back up
# Does not break build if a phone fails to restart
function bb_reboot_phones {
  echo "@@@BUILD_STEP Rebooting phones@@@"
  (
  set +e
  cd $CHROME_SRC/build/android/pylib;
  for DEVICE in $(adb_get_devices); do
    python -c "import android_commands;\
        android_commands.AndroidCommands(device='$DEVICE').Reboot(True)" &
  done
  wait
  )
}

# Runs the license checker for the WebView build.
function bb_check_webview_licenses {
  echo "@@@BUILD_STEP Check licenses for WebView@@@"
  (
  set +e
  cd "${SRC_ROOT}"
  python android_webview/tools/webview_licenses.py scan
  local license_exit_code=$?
  if [[ license_exit_code -ne 0 ]]; then
    echo "@@@STEP_FAILURE@@@"
  fi
  return $license_exit_code
  )
}
