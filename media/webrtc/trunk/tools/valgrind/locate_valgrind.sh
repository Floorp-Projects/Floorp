#!/bin/bash

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Prints a path to Valgrind binaries to be used for Chromium.
# Select the valgrind from third_party/valgrind by default,
# but allow users to override this default without editing scripts and
# without specifying a commandline option

export THISDIR=`dirname $0`

# User may use his own valgrind by giving its path with CHROME_VALGRIND env.
if [ "$CHROME_VALGRIND" = "" ]
then
  # Guess which binaries we should use by uname
  case "$(uname -a)" in
  *Linux*x86_64*)
    PLATFORM="linux_x64"
    ;;
  *Linux*86*)
    PLATFORM="linux_x86"
    ;;
  *Darwin*9.[678].[01]*i386*)
    # Didn't test other kernels.
    PLATFORM="mac"
    ;;
  *Darwin*10.[0-9].[0-9]*i386*)
    PLATFORM="mac_10.6"
    ;;
  *Darwin*10.[0-9].[0-9]*x86_64*)
    PLATFORM="mac_10.6"
    ;;
  *Darwin*11.[0-9].[0-9]*x86_64*)
    PLATFORM="mac_10.7"
    ;;
  *)
    echo "Unknown platform:" >&2
    uname -a >&2
    echo "We'll try to search for valgrind binaries installed in /usr/local" >&2
    PLATFORM=
  esac

  if [ "$PLATFORM" != "" ]
  then
    # The binaries should be in third_party/valgrind
    # (checked out from deps/third_party/valgrind/binaries).
    CHROME_VALGRIND="$THISDIR/../../third_party/valgrind/$PLATFORM"

    # TODO(timurrrr): readlink -f is not present on Mac...
    if [ "$PLATFORM" != "mac" ] && \
      [ "$PLATFORM" != "mac_10.6" ] && \
      [ "$PLATFORM" != "mac_10.7" ]
    then
      # Get rid of all "../" dirs
      CHROME_VALGRIND=`readlink -f $CHROME_VALGRIND`
    fi

    if ! test -x $CHROME_VALGRIND/bin/valgrind
    then
      # We couldn't find the binaries in third_party/valgrind
      CHROME_VALGRIND=""
    fi
  fi
fi

if ! test -x $CHROME_VALGRIND/bin/valgrind
then
  echo "Could not find valgrind binaries anywhere. Please see" >&2
  echo "http://dev.chromium.org/developers/how-tos/using-valgrind/building-valgrind" >&2
  echo "for the instructions on how to get them." >&2
  exit 1
fi

echo $CHROME_VALGRIND
