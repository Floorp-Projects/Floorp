#! /bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
#
# tests/ec/ec.sh
#
# needs to work on all Unix and Windows platforms
# this is a meta script to drive all ec tests
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
########################################################################

############################## run_tests ###############################
# run test suites defined in ECTESTS variable
########################################################################
run_ec_tests()
{
    for ECTEST in ${ECTESTS}
    do
        SCRIPTNAME=${ECTEST}.sh
        echo "Running ec tests for ${ECTEST}"
        echo "TIMESTAMP ${ECTEST} BEGIN: `date`"
        (cd ${QADIR}/ec; . ./${SCRIPTNAME} 2>&1)
        echo "TIMESTAMP ${ECTEST} END: `date`"
    done
}

ECTESTS="ecperf ectest"
run_ec_tests
