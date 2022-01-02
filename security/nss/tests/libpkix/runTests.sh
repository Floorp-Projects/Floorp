#! /bin/sh
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# runTests.sh#
#
# This script enables all tests to be run together. It simply cd's into
# the pkix_tests and pkix_pl_tests directories and runs test scripts
#
# This test is the original of libpkix.sh. While libpkix.sh is invoked by
# all.sh as a /bin/sh script, runTests.sh is a /bin/ksh and provides the
# options of checking memory and using different memory allcation schemes.
#

errors=0
pkixErrors=0
pkixplErrors=0
checkMemArg=""
arenasArg=""
quietArg=""
memText=""

### ParseArgs
ParseArgs() # args
{
    while [ $# -gt 0 ]; do
	if [ $1 = "-checkmem" ]; then
	    checkMemArg=$1
	    memText="   (Memory Checking Enabled)"
	elif [ $1 = "-quiet" ]; then
	    quietArg=$1
	elif [ $1 = "-arenas" ]; then
	    arenasArg=$1
	fi
	shift
    done
}

ParseArgs $*

echo "*******************************************************************************"
echo "START OF ALL TESTS${memText}"
echo "*******************************************************************************"
echo ""

echo "RUNNING tests in pkix_pl_test";
cd pkix_pl_tests;
runPLTests.sh ${arenasArg} ${checkMemArg} ${quietArg}
pkixplErrors=$?

echo "RUNNING tests in pkix_test";
cd ../pkix_tests;
runTests.sh ${arenasArg} ${checkMemArg} ${quietArg}
pkixErrors=$?

echo "RUNNING tests in sample_apps (performance)";
cd ../sample_apps;
runPerf.sh ${arenasArg} ${checkMemArg} ${quietArg}
pkixPerfErrors=$?

errors=`expr ${pkixplErrors} + ${pkixErrors} + ${pkixPerfErrors}`

if [ ${errors} -eq 0 ]; then
    echo ""
    echo "************************************************************"
    echo "END OF ALL TESTS: ALL TESTS COMPLETED SUCCESSFULLY"
    echo "************************************************************"
    exit 0
fi

if [ ${errors} -eq 1 ]; then
    plural=""
else
    plural="S"
fi

echo ""
echo "************************************************************"
echo "END OF ALL TESTS: ${errors} TEST${plural} FAILED"
echo "************************************************************"
exit 1




