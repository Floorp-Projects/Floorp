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
testunit=RESULTS


##########
# main
##########

ParseArgs $*

RunTests <<EOF
pkixutil test_policynode ${NIST} NIST-Test-Files-Used
pkixutil test_valresult ../../certs
pkixutil test_buildresult ../../certs
pkixutil test_verifynode ${NIST} TrustAnchorRootCertificate.crt GoodCACert.crt ValidCertificatePathTest1EE.crt
EOF

totalErrors=$?
html_msg ${totalErrors} 0 "&nbsp;&nbsp;&nbsp;${testunit}: passed ${passed} of ${numtests} tests"
exit ${totalErrors}
