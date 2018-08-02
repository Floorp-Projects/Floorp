#!/bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
#
# tests/bogo/bogo.sh
#
# Script to drive the ssl bogo interop unit tests
#
########################################################################

bogo_init()
{
  SCRIPTNAME="bogo.sh"
  if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ] ; then
    cd ../common
    . ./init.sh
  fi

  mkdir -p "${HOSTDIR}/bogo"
  cd "${HOSTDIR}/bogo"
  BORING=${BORING:=boringssl}
  if [ ! -d "$BORING" ]; then
    git clone -q https://boringssl.googlesource.com/boringssl "$BORING"
    git -C "$BORING" checkout -q 9af1edbe2201e6c6d58e5e484bf56281d8c751d9
  fi

  SCRIPTNAME="bogo.sh"
  html_head "bogo test"
}

bogo_cleanup()
{
  html "</TABLE><BR>"
  cd ${QADIR}
  . common/cleanup.sh
}

cd ../
cwd=$(cd $(dirname $0); pwd -P)
SOURCE_DIR="$cwd"/..
bogo_init
(cd "$BORING"/ssl/test/runner;
 GOPATH="$cwd" go test -pipe -shim-path "${BINDIR}"/nss_bogo_shim \
	 -loose-errors -allow-unimplemented \
	 -shim-config "${SOURCE_DIR}/gtests/nss_bogo_shim/config.json") \
	 2>bogo.errors | tee bogo.log
html_msg "${PIPESTATUS[0]}" 0 "Bogo" "Run successfully"
grep -i 'FAILED\|Assertion failure' bogo.errors
html_msg $? 1 "Bogo" "No failures"
bogo_cleanup
