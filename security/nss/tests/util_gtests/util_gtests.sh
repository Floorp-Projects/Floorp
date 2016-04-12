#! /bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
#
# tests/util_gtests/util_gtests.sh
#
# Script to drive the libnssutil gtest unit tests
#
# needs to work on all Unix and Windows platforms
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
########################################################################

############################## util_gtest_init ##########################
# local shell function to initialize this script
########################################################################
util_gtest_init()
{
  SCRIPTNAME=util_gtest.sh      # sourced - $0 would point to all.sh

  if [ -z "${CLEANUP}" ] ; then     # if nobody else is responsible for
      CLEANUP="${SCRIPTNAME}"       # cleaning this script will do it
  fi
  if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ]; then
      cd ../common
      . ./init.sh
  fi

  SCRIPTNAME=util_gtest.sh
  html_head libnssutil Gtests

  if [ ! -d "${UTILGTESTDIR}" ]; then
    mkdir -p "${UTILGTESTDIR}"
  fi

  cd "${UTILGTESTDIR}"
}

########################## util_gtest_start #########################
# Local function to actually start the test
####################################################################
util_gtest_start()
{
  if [ ! -f ${BINDIR}/util_gtest ]; then
    html_unknown "Skipping util_gtest (not built)"
    return
  fi

  UTILGTESTREPORT="${UTILGTESTDIR}/report.xml"
  ${BINDIR}/util_gtest -d "${UTILGTESTDIR}" --gtest_output=xml:"${UTILGTESTREPORT}"
  html_msg $? 0 "util_gtest run successfully"
  sed -f ${COMMON}/parsegtestreport.sed "${UTILGTESTREPORT}" | \
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

util_gtest_cleanup()
{
  cd ${QADIR}
  . common/cleanup.sh
}

################## main #################################################
cd "$(dirname "$0")"
util_gtest_init
util_gtest_start
util_gtest_cleanup
