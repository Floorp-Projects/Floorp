#! /bin/sh
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# runTests.sh
#

totalErrors=0
pkixErrors=0
pkixplErrors=0
checkMemArg=""
arenasArg=""
quietArg=""
memText=""

############################## libpkix_init ###############################
# local shell function to initialize this script
########################################################################
libpkix_init()
{
  SCRIPTNAME="libpkix.sh"
  if [ -z "${CLEANUP}" ] ; then     # if nobody else is responsible for
      CLEANUP="${SCRIPTNAME}"       # cleaning this script will do it
  fi

  LIBPKIX_CURDIR=`pwd`
  if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ] ; then
      cd ../common
      . ./init.sh
  fi
  cd ${LIBPKIX_CURDIR}

  SCRIPTNAME="libpkix.sh"
}

############################## libpkix_cleanup ############################
# local shell function to finish this script (no exit since it might be
# sourced)
########################################################################
libpkix_cleanup()
{
  html "</TABLE><BR>" 
  cd ${QADIR}
  . common/cleanup.sh
}

############################## libpkix_UT_main ############################
# local shell function to run libpkix unit tests
########################################################################
ParseArgs ()
{
    while [ $# -gt 0 ]; do
        if [ $1 == "-checkmem" ]; then
            checkMemArg=$1
            memText="   (Memory Checking Enabled)"
        elif [ $1 == "-quiet" ]; then
            quietArg=$1
        elif [ $1 == "-arenas" ]; then
            arenasArg=$1
        fi
        shift
    done
}

libpkix_UT_main()
{

html_head "LIBPKIX Unit Tests"

ParseArgs 

echo "*******************************************************************************"
echo "START OF ALL TESTS${memText}"
echo "*******************************************************************************"
echo ""

echo "RUNNING tests in pkix_pl_test";
html_msg 0 0 "Running tests in pkix_pl_test:"
cd pkix_pl_tests;
runPLTests.sh ${arenasArg} ${checkMemArg} ${quietArg}
pkixplErrors=$?
html_msg $? 0 "Results of tests in pkix_pl_test"

echo "RUNNING tests in pkix_test";
html_msg 0 0 "Running tests in pkix_test:"
cd ../pkix_tests;
runTests.sh ${arenasArg} ${checkMemArg} ${quietArg}
pkixErrors=$?
html_msg $? 0 "Results of tests in pkix_test"

echo "RUNNING performance tests in sample_apps";
html_msg 0 0 "Running performance tests in sample_apps:"
cd ../sample_apps;
runPerf.sh ${arenasArg} ${checkMemArg} ${quietArg}
pkixPerfErrors=$?
html_msg $? 0 "Results of performance tests in sample_apps"

totalErrors=`expr ${pkixErrors} + ${pkixplErrors} + ${pkixPerfErrors}`

if [ ${totalErrors} -eq 0 ]; then
    echo ""
    echo "************************************************************"
    echo "END OF ALL TESTS: ALL TESTS COMPLETED SUCCESSFULLY"
    echo "************************************************************"
    html_msg ${totalErrors} 0 "ALL LIBPKIX TESTS COMPLETED SUCCESSFULLY"

    return 0
fi

if [ ${totalErrors} -eq 1 ]; then
    plural=""
else
    plural="S"
fi

if [ ${totalErrors} -ne 0 ]; then
    echo ""
    echo "************************************************************"
    echo "END OF ALL TESTS: ${totalErrors} TEST${plural} FAILED"
    echo "************************************************************"
    html_msg 1 0 "${totalErrors} LIBPKIX TEST${plural} FAILED"
return 1
fi
}

libpkix_run_tests()
{
    if [ -n "${BUILD_LIBPKIX_TESTS}" ]; then
         libpkix_UT_main 
    fi
}

################## main #################################################

libpkix_init 
libpkix_run_tests
libpkix_cleanup
