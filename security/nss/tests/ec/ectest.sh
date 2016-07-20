#! /bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
#
# tests/ec/ectest.sh
#
# needs to work on all Unix and Windows platforms
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
########################################################################

############################## ectest_init #############################
# local shell function to initialize this script
########################################################################

ectest_init()
{
  SCRIPTNAME="ectest.sh"
  if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ] ; then
      cd ../common
      . ./init.sh
  fi
  SCRIPTNAME="ectest.sh"
  html_head "ectest test"
}

ectest_cleanup()
{
  html "</TABLE><BR>"
  cd ${QADIR}
  chmod a+rw $RONLY_DIR
  . common/cleanup.sh
}

ectest_init
ECTEST_OUT=`ectest`
ECTEST_OUT=`echo $ECTEST_OUT | grep -i "not okay"`
# TODO: expose individual tests and failures instead of overall
if [ -n "$ECTEST_OUT" ] ; then
  html_failed "ec(test) test"
else
  html_passed "ec(test) test"
fi
ectest_cleanup
