#! /bin/sh
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




