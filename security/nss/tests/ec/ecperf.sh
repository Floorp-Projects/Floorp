#! /bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
#
# tests/ec/ecperf.sh
#
# needs to work on all Unix and Windows platforms
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
########################################################################

############################## ecperf_init #############################
# local shell function to initialize this script
########################################################################

ecperf_init()
{
  SCRIPTNAME="ecperf.sh"
  if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ] ; then
      cd ../common
      . ./init.sh
  fi
  SCRIPTNAME="ecperf.sh"
  html_head "ecperf test"
}

ecperf_cleanup()
{
  html "</TABLE><BR>"
  cd ${QADIR}
  chmod a+rw $RONLY_DIR
  . common/cleanup.sh
}

ecperf_init
ECPERF_OUT=`ecperf`
ECPERF_OUT=`echo $ECPERF_OUT | grep -i "failed"`
# TODO: this is a perf test we don't check for performance here but only failed
if [ -n "$ECPERF_OUT" ] ; then
  html_failed "ec(perf) test"
else
  html_passed "ec(perf) test"
fi
ecperf_cleanup