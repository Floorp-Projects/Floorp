#!/bin/sh

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to run tests under tools/valgrind/chrome_tests.sh
# in a loop looking for rare/flaky valgrind warnings, and
# generate suppressions for them, to be later filed as bugs
# and added to our suppressions file.
#
# FIXME: Layout tests are a bit funny - they have their own
# sharding control, and should probably be tweaked to obey
# GTEST_SHARD_INDEX/GTEST_TOTAL_SHARDS like the rest,
# but they take days and days to run, so they are left
# out of this script.

if test ! -d chrome
then
  echo "Please run from parent directory of chrome and build directories"
  exit 1
fi

if test "$1" = ""
then
  echo "Usage: shard-all-tests.sh [BUILDTYPE=Release] target [target ...]"
  echo "Example: shard-all-tests.sh ui_tests"
  exit 1
fi

set -x
set -e

# Regexp to match any valgrind error
PATTERN="ERROR SUMMARY: [^0]|are definitely|uninitialised|Unhandled exception|\
Invalid read|Invalid write|Invalid free|Source and desti|Mismatched free|\
unaddressable byte|vex x86|impossible|Assertion|INTERNAL ERROR|finish writing|OUCH"

BUILDTYPE=Debug
case "$1" in
  BUILDTYPE=Debug) BUILDTYPE=Debug ; shift ;;
  BUILDTYPE=Release) BUILDTYPE=Release ; shift ;;
  BUILDTYPE=*) echo "unknown build type $1"; exit 1;;
  *) ;;
esac
TESTS="$@"

what_to_build() {
  echo $TESTS | tr ' ' '\012' | grep -v layout_tests || true
  echo $TESTS | grep -q layout_tests && echo test_shell || true
  echo $TESTS | grep -q ui_tests && echo chrome || true
}

# Wrap xcodebuild to take same arguments as our make, more or less
xcodemake() {
  for target in $*
  do
    case $target in
    chrome)         xcodebuild -configuration $BUILDTYPE -project chrome/chrome.xcodeproj -target chrome ;;
    ui_tests)       xcodebuild -configuration $BUILDTYPE -project chrome/chrome.xcodeproj -target ui_tests ;;
    base_unittests) xcodebuild -configuration $BUILDTYPE -project base/base.xcodeproj     -target base_unittests ;;
    net_unittests)  xcodebuild -configuration $BUILDTYPE -project net/net.xcodeproj       -target net_unittests ;;
    *) echo "dunno how to build $target yet"; exit 1 ;;
    esac
  done
}

build_tests() {
  buildtype=$1
  shift

  OS=`uname`
  case $OS in
  Linux)
    # Lame way to autodetect whether 'make' or 'hammer' is in use
    if test -d out
    then
      make -j4 BUILDTYPE=$1 $@
    else
      # fixme: obey buildtype
      hammer $@
    fi
    ;;
  Darwin)
    xcodemake $@
    ;;
  *) echo "don't know how to build on os $OS"
    ;;
  esac
}

TESTS_BUILDABLE=`what_to_build`
echo building $TESTS_BUILDABLE
build_tests $BUILDTYPE $TESTS_BUILDABLE

# Divide each test suite up into 100 shards, as first step
# in tracking down exact source of errors.
export GTEST_TOTAL_SHARDS=100

rm -rf *.vlog *.vtmp || true

iter=0
while test $iter -lt 1000
do
  for testname in $TESTS
  do
    export GTEST_SHARD_INDEX=0
    while test $GTEST_SHARD_INDEX -lt $GTEST_TOTAL_SHARDS
    do
      i=$GTEST_SHARD_INDEX
      sh tools/valgrind/chrome_tests.sh -b xcodebuild/$BUILDTYPE -t ${testname} --tool_flags="--nocleanup_on_exit" > ${testname}_$i.vlog 2>&1 || true
      mv valgrind.tmp ${testname}_$i.vtmp
      GTEST_SHARD_INDEX=`expr $GTEST_SHARD_INDEX + 1`
    done
  done

  # Save any interesting log files from this iteration
  # Also show interesting lines on stdout, to make tail -f more interesting
  if egrep "$PATTERN" *.vlog
  then
    mkdir -p shard-results/$iter
    mv `egrep -l "$PATTERN" *.vlog` shard-results/$iter
    # ideally we'd only save the .vtmp's corresponding to the .vlogs we saved
    mv *.vtmp shard-results/$iter
  fi

  rm -rf *.vlog *.vtmp || true
  iter=`expr $iter + 1`
done
