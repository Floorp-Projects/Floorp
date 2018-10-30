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
    git clone -q https://github.com/mozilla/tls-interop "$INTEROP"
    git -C "$INTEROP" checkout -q c00685aa953c49f1e844e614746aadc783e81b19
  fi
  INTEROP=$(cd "$INTEROP";pwd -P)

  # We use the BoringSSL keyfiles
  BORING=${BORING:=boringssl}
  if [ ! -d "$BORING" ]; then
    git clone -q https://boringssl.googlesource.com/boringssl "$BORING"
    git -C "$BORING" checkout -q 7f4f41fa81c03e0f8ef1ab5b3d1d566b5968f107
  fi
  BORING=$(cd "$BORING";pwd -P)
  mkdir "$BORING/build"
  cd "$BORING/build"
  
  # Build boring explicitly with gcc because it fails on builds where 
  # CC=clang-5.0, for example on asan-builds.
  export CC=gcc
  cmake ..
  make -j$(nproc)

  # Check out and build OpenSSL. 
  # Build with "enable-external-tests" to include the shim in the build. 
  cd "${HOSTDIR}"
  OSSL=${OSSL:=openssl}
  if [ ! -d "$OSSL" ]; then
    git clone -q https://github.com/openssl/openssl.git "$OSSL"
    git -C "$OSSL" checkout -q 7d38ca3f8bca58bf7b69e78c1f1ab69e5f429dff
  fi
  OSSL=$(cd "$OSSL";pwd -P)
  cd "$OSSL"
  ./config enable-external-tests
  make -j$(nproc)

  #Some filenames in the OpenSSL repository contain "core".
  #This prevents false positive "core file detected" errors.
  detect_core

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
   cargo run -- --client "$client" --server "$server" --rootdir "$BORING"/ssl/test/runner/ --test-cases cases.json $4 $5 ) 2>interop-${test_name}.errors | tee interop-${test_name}.log
  RESULT=${PIPESTATUS[0]}
  html_msg "${RESULT}" 0 "Interop ${test_name}" "Run successfully"
  if [ $RESULT -ne 0 ]; then
    cat interop-${test_name}.errors
    cat interop-${test_name}.log
  fi
  grep -i 'FAILED\|Assertion failure' interop-${test_name}.errors
  html_msg $? 1 "Interop ${test_name}" "No failures"
}

cd "$(dirname "$0")"
interop_init
NSS_SHIM="$BINDIR"/nss_bogo_shim
BORING_SHIM="$BORING"/build/ssl/test/bssl_shim
OSSL_SHIM="$OSSL"/test/ossl_shim/ossl_shim
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH":"$OSSL"
interop_run "nss_nss" ${NSS_SHIM} ${NSS_SHIM}
interop_run "bssl_nss" ${BORING_SHIM} ${NSS_SHIM}
interop_run "nss_bssl" ${NSS_SHIM} ${BORING_SHIM} "--client-writes-first"
interop_run "ossl_nss" ${OSSL_SHIM} ${NSS_SHIM} "--force-IPv4"
interop_run "nss_ossl" ${NSS_SHIM} ${OSSL_SHIM} "--client-writes-first" "--force-IPv4"
interop_cleanup
