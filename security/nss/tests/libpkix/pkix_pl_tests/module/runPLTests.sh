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
. ./libpkix_init_nist.sh 
cd ${curdir}

numtests=0
passed=0
testunit=MODULE
doModule=1

### setup NIST files need to link in
linkModuleNistFiles="InvalidDNnameConstraintsTest3EE.crt 
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
 
     for i in ${linkModuleNistFiles}; do
         if [ -f ${HOSTDIR}/rev_data/local/$i ]; then
             rm ${HOSTDIR}/rev_data/local/$i
         fi
         cp ${NIST_FILES_DIR}/$i ${HOSTDIR}/rev_data/local/$i
     done

    localCRLFiles="crlgood.crl
	crldiff.crl
	issuer-hanfei.crl
	issuer-none.crl"

    for i in ${localCRLFiles}; do
        cp ${curdir}/rev_data/local/$i ${HOSTDIR}/rev_data/local/$i
    done
fi

##########
# main
##########

ParseArgs $*

SOCKETTRACE=0
export SOCKETTRACE

RunTests <<EOF
pkixutil test_colcertstore NIST-Test-Files-Used rev_data/local ${HOSTDIR}
pkixutil test_pk11certstore -d ../../pkix_pl_tests/module ../../pkix_tests/top/rev_data/crlchecker
pkixutil test_ekuchecker "Test-EKU-without-OID" ENE "" rev_data test_eku_codesigning_clientauth.crt test_eku_clientauth.crt test_eku_clientauthEE.crt
pkixutil test_ekuchecker "Test-EKU-with-good-OID" ENE "1.3.6.1.5.5.7.3.3" rev_data test_eku_codesigning_clientauth.crt test_eku_clientauth.crt test_eku_clientauthEE.crt 
pkixutil test_ekuchecker "Test-EKU-with-bad-OID" EE "1.3.6.1.5.5.7.3.4" rev_data test_eku_codesigning_clientauth.crt test_eku_clientauth.crt test_eku_clientauthEE.crt 
pkixutil test_ekuchecker "Test-EKU-with-good-and-bad-OID" EE "1.3.6.1.5.5.7.3.3,1.3.6.1.5.5.7.3.4" rev_data test_eku_codesigning_clientauth.crt test_eku_clientauth.crt test_eku_clientauthEE.crt
pkixutil test_ekuchecker "Test-EKU-only-EE-with-good-OID" ENE "E1.3.6.1.5.5.7.3.3" rev_data test_eku_codesigning_clientauth.crt test_eku_clientauth.crt test_eku_clientauthEE.crt
pkixutil test_ekuchecker "Test-EKU-only-EE-with-bad-OID" EE "E1.3.6.1.5.5.7.3.4" rev_data test_eku_codesigning_clientauth.crt test_eku_clientauth.crt test_eku_clientauthEE.crt
pkixutil test_ekuchecker "Test-EKU-serverAuth" ENE "1.3.6.1.5.5.7.3.1" rev_data test_eku_all.crt test_eku_allbutcodesigningEE.crt
pkixutil test_ekuchecker "Test-EKU-clientAuth" ENE "1.3.6.1.5.5.7.3.2" rev_data test_eku_all.crt test_eku_allbutcodesigningEE.crt
pkixutil test_ekuchecker "Test-EKU-codesigning-without-OID" EE "1.3.6.1.5.5.7.3.3" rev_data test_eku_all.crt test_eku_allbutcodesigningEE.crt
pkixutil test_ekuchecker "Test-EKU-emailProtection" ENE "1.3.6.1.5.5.7.3.4" rev_data test_eku_all.crt test_eku_allbutcodesigningEE.crt
pkixutil test_ekuchecker "Test-EKU-timestamping" ENE "1.3.6.1.5.5.7.3.8" rev_data test_eku_all.crt test_eku_allbutcodesigningEE.crt
pkixutil test_ekuchecker "Test-EKU-OCSPSigning" ENE "1.3.6.1.5.5.7.3.9" rev_data test_eku_all.crt test_eku_allbutcodesigningEE.crt
pkixutil test_ekuchecker "Test-EKU-only-EE-serverAuth" ENE "E1.3.6.1.5.5.7.3.1" rev_data test_eku_all.crt test_eku_allbutcodesigningEE.crt
pkixutil test_ekuchecker "Test-EKU-only-EE-clientAuth" ENE "E1.3.6.1.5.5.7.3.2" rev_data test_eku_all.crt test_eku_allbutcodesigningEE.crt
pkixutil test_ekuchecker "Test-EKU-only-EE-codesigning-without-OID" EE "E1.3.6.1.5.5.7.3.3" rev_data test_eku_all.crt test_eku_allbutcodesigningEE.crt
pkixutil test_ekuchecker "Test-EKU-only-EE-emailProtection" ENE "E1.3.6.1.5.5.7.3.4" rev_data test_eku_all.crt test_eku_allbutcodesigningEE.crt
pkixutil test_ekuchecker "Test-EKU-only-EE-timestamping" ENE "E1.3.6.1.5.5.7.3.8" rev_data test_eku_all.crt test_eku_allbutcodesigningEE.crt
pkixutil test_ekuchecker "Test-EKU-only-EE-ocspSigning" ENE "E1.3.6.1.5.5.7.3.9" rev_data test_eku_all.crt test_eku_allbutcodesigningEE.crt
pkixutil test_socket ${HOSTADDR}:2000
EOF

totalErrors=$?
html_msg ${totalErrors} 0 "&nbsp;&nbsp;&nbsp;${testunit}: passed ${passed} of ${numtests} tests"
exit ${totalErrors}

