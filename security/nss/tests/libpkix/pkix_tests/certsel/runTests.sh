#!/bin/sh
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# runTests.sh
#

curdir=`pwd`
cd ../../common
. ./libpkix_init.sh > /dev/null
. ./libpkix_init_nist.sh
cd ${curdir}

numtests=0
passed=0
testunit=CERTSEL

##########
# main
##########

ParseArgs $*

RunTests <<EOF
pkixutil test_comcertselparams ${NIST} NIST-Test-Files-Used
pkixutil test_certselector ${NIST} NIST-Test-Files-Used ../../pkix_pl_tests/module/rev_data
EOF

totalErrors=$?
html_msg ${totalErrors} 0 "&nbsp;&nbsp;&nbsp;${testunit}: passed ${passed} of ${numtests} tests"
exit ${totalErrors}
