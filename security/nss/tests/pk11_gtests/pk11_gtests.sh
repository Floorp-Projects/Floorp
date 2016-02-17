#! /bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
#
# tests/pk11_gtests/pk11_gtests.sh
#
# Script to drive the PKCS#11 gtest unit tests
#
# needs to work on all Unix and Windows platforms
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
########################################################################

############################## pk11_gtest_init ##########################
# local shell function to initialize this script
########################################################################
pk11_gtest_init()
{
  SCRIPTNAME=pk11_gtest.sh      # sourced - $0 would point to all.sh

  if [ -z "${CLEANUP}" ] ; then     # if nobody else is responsible for
      CLEANUP="${SCRIPTNAME}"       # cleaning this script will do it
  fi
  if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ]; then
      cd ../common
      . ./init.sh
  fi

  SCRIPTNAME=pk11_gtest.sh
  html_head PKCS\#11 Gtests

  if [ ! -d "${PK11GTESTDIR}" ]; then
    mkdir -p "${PK11GTESTDIR}"
  fi

  cd "${PK11GTESTDIR}"
}

########################## pk11_gtest_start #########################
# Local function to actually start the test
####################################################################
pk11_gtest_start()
{
  if [ ! -f ${BINDIR}/pk11_gtest ]; then
    html_unknown "Skipping pk11_gtest (not built)"
    return
  fi

  # Temporarily disable asserts for PKCS#11 slot leakage (Bug 1168425)
  unset NSS_STRICT_SHUTDOWN
  PK11GTESTREPORT="${PK11GTESTDIR}/report.xml"
  ${BINDIR}/pk11_gtest -d "${PK11GTESTDIR}" --gtest_output=xml:"${PK11GTESTREPORT}"
  html_msg $? 0 "pk11_gtest run successfully"
  sed -f ${COMMON}/parsegtestreport.sed "${PK11GTESTREPORT}" | \
  while read result name; do
    if [ "$result" = "notrun" ]; then
      echo "$name" SKIPPED
    elif [ "$result" = "run" ]; then
      html_passed "$name" > /dev/null
    else
      html_failed "$name"
    fi
  done
}

pk11_gtest_cleanup()
{
  cd ${QADIR}
  . common/cleanup.sh
}

################## main #################################################
cd "$(dirname "$0")"
pk11_gtest_init
pk11_gtest_start
pk11_gtest_cleanup
