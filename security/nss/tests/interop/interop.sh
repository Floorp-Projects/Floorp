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
    git clone -q https://github.com/ttaubert/tls-interop "$INTEROP"
    git -C "$INTEROP" checkout -q 07930b791827c1bdb6f4c19ca0aa63850fd59e22
  fi
  INTEROP=$(cd "$INTEROP";pwd -P)

  # We use the BoringSSL keyfiles
  BORING=${BORING:=boringssl}
  if [ ! -d "$BORING" ]; then
    git clone -q https://boringssl.googlesource.com/boringssl "$BORING"
    git -C "$BORING" checkout -q ea80f9d5df4c302de391e999395e1c87f9c786b3
  fi
  BORING=$(cd "$BORING";pwd -P)

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

  (cd "$INTEROP";
   cargo run -- --client "$client" --server "$server" --rootdir "$BORING"/ssl/test/runner/ --test-cases cases.json) 2>interop-${test_name}.errors | tee interop-${test_name}.log
  html_msg "${PIPESTATUS[0]}" 0 "Interop" "Run successfully"
  grep -i 'FAILED\|Assertion failure' interop-${test_name}.errors
  html_msg $? 1 "Interop" "No failures"
}

cd "$(dirname "$0")"
SOURCE_DIR="$PWD"/../..
interop_init
NSS_SHIM="$BINDIR"/nss_bogo_shim
BORING_SHIM="$BORING"/build/ssl/test/bssl_shim
interop_run "nss_nss" ${NSS_SHIM} ${NSS_SHIM}
interop_cleanup
