#!/bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
#
# tests/interop/interop.sh
#
# Script to drive our cross-stack interop tests
#
########################################################################

interop_init()
{
  SCRIPTNAME="interop.sh"
  if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ] ; then
    cd ../common
    . ./init.sh
  fi

  mkdir -p "${HOSTDIR}/interop"
  cd "${HOSTDIR}/interop"
  INTEROP=${INTEROP:=tls_interop}
  if [ ! -d "$INTEROP" ]; then
    git clone -q https://github.com/jallmann/tls-interop "$INTEROP"
    git -C "$INTEROP" checkout -q befbbde4e9e3ff78a6f90dc3170755bddcb1a2ef
  fi
  INTEROP=$(cd "$INTEROP";pwd -P)

  # We use the BoringSSL keyfiles
  BORING=${BORING:=boringssl}
  if [ ! -d "$BORING" ]; then
    git clone -q https://boringssl.googlesource.com/boringssl "$BORING"
    git -C "$BORING" checkout -q 3815720cf31339b1739f8ad1c21205105412c6b5
  fi
  BORING=$(cd "$BORING";pwd -P)
  mkdir "$BORING/build"
  cd "$BORING/build"
  
  # Build boring explicitly with gcc because it fails on builds where 
  # CC=clang-5.0, for example asan-builds.
  export CC=gcc
  cmake ..
  make

  SCRIPTNAME="interop.sh"
  html_head "interop test"
}

interop_cleanup()
{
  html "</TABLE><BR>"
  cd ${QADIR}
  . common/cleanup.sh
}

# Function so we can easily add other stacks
interop_run()
{
  test_name=$1
  client=$2
  server=$3
  client_writes_first=$4

  (cd "$INTEROP";
   cargo run -- --client "$client" --server "$server" --rootdir "$BORING"/ssl/test/runner/ --test-cases cases.json $client_writes_first ) 2>interop-${test_name}.errors | tee interop-${test_name}.log
  RESULT=${PIPESTATUS[0]}
  html_msg "${RESULT}" 0 "Interop" "Run successfully"
  if [ $RESULT -ne 0 ]; then
    cat interop-${test_name}.errors
    cat interop-${test_name}.log
  fi
  grep -i 'FAILED\|Assertion failure' interop-${test_name}.errors
  html_msg $? 1 "Interop" "No failures"
}

interop_init
NSS_SHIM="$BINDIR"/nss_bogo_shim
BORING_SHIM="$BORING"/build/ssl/test/bssl_shim
interop_run "nss_nss" ${NSS_SHIM} ${NSS_SHIM}
interop_run "bssl_nss" ${BORING_SHIM} ${NSS_SHIM}
interop_run "nss_bssl" ${NSS_SHIM} ${BORING_SHIM} "--client-writes-first"
interop_cleanup
