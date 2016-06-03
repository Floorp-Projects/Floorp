#! /bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
#
# similar to all.sh this file runs drives gtests.
#
# needs to work on all Unix and Windows platforms
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
########################################################################

############################## gtest_init ##############################
# local shell function to initialize this script
########################################################################
gtest_init()
{
  cd "$(dirname "$1")"
  if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ]; then
      cd common
      . ./init.sh
  fi

  SCRIPTNAME=gtests.sh

  if [ -z "${CLEANUP}" ] ; then   # if nobody else is responsible for
    CLEANUP="${SCRIPTNAME}"       # cleaning this script will do it
  fi
}

########################## gtest_start #############################
# Local function to actually start the test
####################################################################
gtest_start()
{
  echo "gtests: ${GTESTS}"
  for i in ${GTESTS}; do
    GTESTDIR="${HOSTDIR}/$i"
    html_head "$i"
    if [ ! -d "$GTESTDIR" ]; then
      mkdir -p "$GTESTDIR"
    fi
    cd "$GTESTDIR"
    GTESTREPORT="$GTESTDIR/report.xml"
    ${BINDIR}/$i -d "$GTESTDIR" --gtest_output=xml:"${GTESTREPORT}"
    echo "test output dir: ${GTESTREPORT}"
    html_msg $? 0 "$i run successfully"
    sed -f ${COMMON}/parsegtestreport.sed "${GTESTREPORT}" | \
    while read result name; do
      if [ "$result" = "notrun" ]; then
        echo "$name" SKIPPED
      elif [ "$result" = "run" ]; then
        html_passed "$name" > /dev/null
      else
        html_failed "$name"
      fi
    done
  done
}

gtest_cleanup()
{
  html "</TABLE><BR>"
  cd ${QADIR}
  . common/cleanup.sh
}

################## main #################################################
GTESTS="der_gtest pk11_gtest util_gtest"
gtest_init $0
gtest_start
gtest_cleanup
