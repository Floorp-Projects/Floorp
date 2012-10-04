#!/bin/bash -e

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to install everything needed to build chromium on android that
# requires sudo privileges.
# See http://code.google.com/p/chromium/wiki/AndroidBuildInstructions

# This script installs the sun-java6 packages (bin, jre and jdk). Sun requires
# a license agreement, so upon installation it will prompt the user. To get
# past the curses-based dialog press TAB <ret> TAB <ret> to agree.

if ! uname -m | egrep -q "i686|x86_64"; then
  echo "Only x86 architectures are currently supported" >&2
  exit
fi

if [ "x$(id -u)" != x0 ]; then
  echo "Running as non-root user."
  echo "You might have to enter your password one or more times for 'sudo'."
  echo
fi

# The temporary directory used to store output of update-java-alternatives
TEMPDIR=$(mktemp -d)
cleanup() {
  local status=${?}
  trap - EXIT
  rm -rf "${TEMPDIR}"
  exit ${status}
}
trap cleanup EXIT

sudo apt-get update

# Fix deps
sudo apt-get -f install

# Install deps
sudo apt-get -y install python-pexpect ant1.8 xvfb x11-utils

# Install sun-java6 stuff
sudo apt-get -y install sun-java6-bin sun-java6-jre sun-java6-jdk

# Switch version of Java to java-6-sun
# Sun's java is missing certain Java plugins (e.g. for firefox, mozilla).  These
# are not required to build, and thus are treated only as warnings. Any errors
# in updating java alternatives which are not '*-javaplugin.so' will cause
# errors and stop the script from completing successfully.
if ! sudo update-java-alternatives -s java-6-sun \
          >& "${TEMPDIR}"/update-java-alternatives.out
then
  # Check that there are the expected javaplugin.so errors for the update
  if grep 'javaplugin.so' "${TEMPDIR}"/update-java-alternatives.out >& /dev/null
  then
    # Print as warnings all the javaplugin.so errors
    echo 'WARNING: java-6-sun has no alternatives for the following plugins:'
    grep 'javaplugin.so' "${TEMPDIR}"/update-java-alternatives.out
  fi
  # Check if there are any errors that are not javaplugin.so
  if grep -v 'javaplugin.so' "${TEMPDIR}"/update-java-alternatives.out \
         >& /dev/null
  then
    # If there are non-javaplugin.so errors, treat as errors and exit
    echo 'ERRORS: Failed to update alternatives for java-6-sun:'
    grep -v 'javaplugin.so' "${TEMPDIR}"/update-java-alternatives.out
    exit 1
  fi
fi

echo "install-build-deps-android.sh complete."
