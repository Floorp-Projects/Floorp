#!/bin/sh
# 
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Netscape security libraries.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1994-2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****
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

