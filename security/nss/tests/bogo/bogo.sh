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

# Currently used BorringSSL version
BOGO_VERSION=f5e0c8f92a22679b0cd8d24d0d670769c1cc07f3

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
    git -C "$BORING" checkout -q $BOGO_VERSION
  fi

  SCRIPTNAME="bogo.sh"
  html_head "bogo test  "
  html_msg $? 0 "Bogo" "Checking out BoringSSL revision $BOGO_VERSION"
}

bogo_cleanup()
{
  html "</TABLE><BR>"
  cd ${QADIR}
  . common/cleanup.sh
}

cd "$(dirname "$0")"
cwd=$(pwd -P)
SOURCE_DIR="$(cd "$cwd"/../..; pwd -P)"
bogo_init
(cd "$BORING"/ssl/test/runner;
git apply ${SOURCE_DIR}/gtests/nss_bogo_shim/nss_loose_local_errors.patch)
html_msg $? 0 "Bogo" "NSS -loose-local-errors patch application"
html_head "bogo log   "
echo ""
(cd "$BORING"/ssl/test/runner;
GOPATH="$cwd" go test -pipe -shim-path "${BINDIR}"/nss_bogo_shim \
	 -loose-errors -loose-local-errors -allow-unimplemented \
     -shim-config "${SOURCE_DIR}/gtests/nss_bogo_shim/config.json") \
	 2>bogo.errors | tee bogo.log | grep -v 'UNIMPLEMENTED'
RES="${PIPESTATUS[0]}"
html_head "bogo result"
html_msg $RES 0 "Bogo" "Test Run"
bogo_cleanup
