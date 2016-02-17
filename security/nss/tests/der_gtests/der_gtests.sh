#! /bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
#
# tests/der_gtests/der_gtests.sh
#
# Script to drive the DER unit gtests
#
# needs to work on all Unix and Windows platforms
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
########################################################################

############################## der_gtest_init ##########################
# local shell function to initialize this script
########################################################################
der_gtest_init()
{
  SCRIPTNAME=der_gtest.sh      # sourced - $0 would point to all.sh

  if [ -z "${CLEANUP}" ] ; then     # if nobody else is responsible for
      CLEANUP="${SCRIPTNAME}"       # cleaning this script will do it
  fi
  if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ]; then
      cd ../common
      . ./init.sh
  fi

  SCRIPTNAME=der_gtest.sh
  html_head DER Gtests

  if [ ! -d "${DERGTESTDIR}" ]; then
    mkdir -p "${DERGTESTDIR}"
  fi

  cd "${DERGTESTDIR}"
}

########################## der_gtest_start #########################
# Local function to actually start the test
####################################################################
der_gtest_start()
{
  if [ ! -f ${BINDIR}/der_gtest ]; then
    html_unknown "Skipping der_gtest (not built)"
    return
  fi

  # Temporarily disable asserts for PKCS#11 slot leakage (Bug 1168425)
  unset NSS_STRICT_SHUTDOWN
  DERGTESTREPORT="${DERGTESTDIR}/report.xml"
  ${BINDIR}/der_gtest -d "${DERGTESTDIR}" --gtest_output=xml:"${DERGTESTREPORT}"
  html_msg $? 0 "der_gtest run successfully"
  sed -f ${COMMON}/parsegtestreport.sed "${DERGTESTREPORT}" | \
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

der_gtest_cleanup()
{
  cd ${QADIR}
  . common/cleanup.sh
}

################## main #################################################
cd "$(dirname "$0")"
der_gtest_init
der_gtest_start
der_gtest_cleanup
