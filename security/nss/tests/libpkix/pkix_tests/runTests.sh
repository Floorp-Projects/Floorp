#!/bin/sh
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# runTests.sh
#

curdir=`pwd`
cd ../common
. ./libpkix_init.sh > /dev/null
cd ${curdir}

testunit="PKIX"

totalErrors=0
utilErrors=0
crlselErrors=0
paramsErrors=0
resultsErrors=0
topErrors=0
checkerErrors=0
certselErrors=0
quiet=0

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
            quiet=1
        elif [ $1 = "-arenas" ]; then
            arenasArg=$1
        fi
        shift
    done
}

ParseArgs $*

testHeadingEcho

echo "RUNNING tests in certsel";
cd certsel;
runTests.sh ${arenasArg} ${checkMemArg} ${quietArg}
certselErrors=$?

echo "RUNNING tests in checker";
cd ../checker;
runTests.sh ${arenasArg} ${checkMemArg} ${quietArg}
checkerErrors=$?

echo "RUNNING tests in results";
cd ../results;
runTests.sh ${arenasArg} ${checkMemArg} ${quietArg}
resultsErrors=$?

echo "RUNNING tests in params";
cd ../params;
runTests.sh ${arenasArg} ${checkMemArg} ${quietArg}
paramsErrors=$?

echo "RUNNING tests in crlsel";
cd ../crlsel;
runTests.sh ${arenasArg} ${checkMemArg} ${quietArg}
crlselErrors=$?

echo "RUNNING tests in store";
cd ../store;
runTests.sh ${arenasArg} ${checkMemArg} ${quietArg}
storeErrors=$?

echo "RUNNING tests in util";
cd ../util;
runTests.sh ${arenasArg} ${checkMemArg} ${quietArg}
utilErrors=$?

echo "RUNNING tests in top";
cd ../top;
runTests.sh ${arenasArg} ${checkMemArg} ${quietArg}
topErrors=$?

totalErrors=`expr ${certselErrors} + ${checkerErrors} + ${resultsErrors} + ${paramsErrors} + ${crlselErrors} + ${storeErrors} + ${utilErrors} + ${topErrors}`

testEndingEcho

exit ${totalErrors}

