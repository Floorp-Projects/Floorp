#!/bin/sh
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# runPLTests.sh
#

curdir=`pwd`
cd ../common
. ./libpkix_init.sh > /dev/null
cd ${curdir}

testunit="PKIX_PL"

totalErrors=0
moduleErrors=0
systemErrors=0
pkiErrors=0
quiet=0

checkMemArg=""
arenasArg=""
quietArg=""

### ParseArgs
myParseArgs() # args
{
    while [ $# -gt 0 ]; do
        if [ $1 = "-checkmem" ]; then
            checkMemArg=$1
        elif [ $1 = "-quiet" ]; then
            quietArg=$1
            quiet=1
        elif [ $1 = "-arenas" ]; then
            arenasArg=$1
        fi
        shift
    done
}

myParseArgs $*

testHeadingEcho

echo "RUNNING tests in pki";
cd pki;
runPLTests.sh ${arenasArg} ${checkMemArg} ${quietArg}
pkiErrors=$?

echo "RUNNING tests in system";
cd ../system;
runPLTests.sh ${arenasArg} ${checkMemArg} ${quietArg}
systemErrors=$?

echo "RUNNING tests in module";
cd ../module;
runPLTests.sh ${arenasArg} ${checkMemArg} ${quietArg}
moduleErrors=$?

totalErrors=`expr $moduleErrors + $systemErrors + $pkiErrors`

testEndingEcho

exit ${totalErrors}

