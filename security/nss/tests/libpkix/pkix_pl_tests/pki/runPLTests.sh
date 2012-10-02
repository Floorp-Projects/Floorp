#!/bin/sh
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# runPLTests.sh
#

curdir=`pwd`
cd ../../common
. ./libpkix_init.sh > /dev/null
doPD=1
. ./libpkix_init_nist.sh
cd ${curdir}

numtests=0
passed=0
testunit=PKI
doPki=1

### setup NIST files need to link in
linkPkiNistFiles="InvalidDNnameConstraintsTest3EE.crt 
        InvalidonlySomeReasonsTest21EE.crt 
        indirectCRLCA3cRLIssuerCRL.crl  
        nameConstraintsDN3subCA2Cert.crt 
        nameConstraintsDN4CACert.crt 
        nameConstraintsDN5CACert.crt 
        onlyContainsAttributeCertsCACRL.crl 
        onlyContainsCACertsCACRL.crl 
        onlyContainsUserCertsCACRL.crl 
        onlySomeReasonsCA3compromiseCRL.crl
        requireExplicitPolicy2CACert.crt 
        inhibitPolicyMapping5CACert.crt 
        inhibitAnyPolicy5CACert.crt 
        inhibitAnyPolicy0CACert.crt 
        P1Mapping1to234CACert.crt 
        UserNoticeQualifierTest15EE.crt 
        UserNoticeQualifierTest16EE.crt 
        UserNoticeQualifierTest17EE.crt 
        UserNoticeQualifierTest18EE.crt 
        CPSPointerQualifierTest20EE.crt"

if [ -n "${NIST_FILES_DIR}" ]; then
    if [ ! -d ${HOSTDIR}/rev_data/local ]; then
        mkdir -p ${HOSTDIR}/rev_data/local
    fi

    for i in ${linkPkiNistFiles}; do
        if [ -f ${HOSTDIR}/rev_data/local/$i ]; then
            rm ${HOSTDIR}/rev_data/local/$i
        fi
        cp ${NIST_FILES_DIR}/$i ${HOSTDIR}/rev_data/local/$i
    done
fi

##########
# main
#########

TZ=US/Eastern

ParseArgs $*

RunTests <<EOF
pkixutil test_cert NIST-Test-Files-Used ../../certs ${HOSTDIR}/rev_data/local
pkixutil test_crl NIST-Test-Files-Used ../../certs
pkixutil test_x500name
pkixutil test_generalname
pkixutil test_date NIST-Test-Files-Used
pkixutil test_crlentry ../../certs
pkixutil test_nameconstraints NIST-Test-Files-Used rev_data/local ${HOSTDIR}
pkixutil test_authorityinfoaccess NIST-PDTest ${NIST_PDTEST} certs/BasicLDAPURIPathDiscoveryOU1EE1.crt certs/BasicHTTPURITrustAnchorRootCert.crt
pkixutil test_subjectinfoaccess NIST-PDTest ${NIST_PDTEST} certs/BasicHTTPURITrustAnchorRootCert.crt certs/BasicLDAPURIPathDiscoveryOU1EE1.crt
EOF

totalErrors=$?
html_msg ${totalErrors} 0 "&nbsp;&nbsp;&nbsp;${testunit}: passed ${passed} of ${numtests} tests"
exit ${totalErrors}


