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
test_colcertstore NIST-Test-Files-Used rev_data/local ${HOSTDIR}
test_pk11certstore ../../pkix_pl_tests/module ../../pkix_tests/top/rev_data/crlchecker
test_ekuchecker "Test-EKU-without-OID" ENE "" rev_data test_eku_codesigning_clientauth.crt test_eku_clientauth.crt test_eku_clientauthEE.crt
test_ekuchecker "Test-EKU-with-good-OID" ENE "1.3.6.1.5.5.7.3.3" rev_data test_eku_codesigning_clientauth.crt test_eku_clientauth.crt test_eku_clientauthEE.crt 
test_ekuchecker "Test-EKU-with-bad-OID" EE "1.3.6.1.5.5.7.3.4" rev_data test_eku_codesigning_clientauth.crt test_eku_clientauth.crt test_eku_clientauthEE.crt 
test_ekuchecker "Test-EKU-with-good-and-bad-OID" EE "1.3.6.1.5.5.7.3.3,1.3.6.1.5.5.7.3.4" rev_data test_eku_codesigning_clientauth.crt test_eku_clientauth.crt test_eku_clientauthEE.crt
test_ekuchecker "Test-EKU-only-EE-with-good-OID" ENE "E1.3.6.1.5.5.7.3.3" rev_data test_eku_codesigning_clientauth.crt test_eku_clientauth.crt test_eku_clientauthEE.crt
test_ekuchecker "Test-EKU-only-EE-with-bad-OID" EE "E1.3.6.1.5.5.7.3.4" rev_data test_eku_codesigning_clientauth.crt test_eku_clientauth.crt test_eku_clientauthEE.crt
test_ekuchecker "Test-EKU-serverAuth" ENE "1.3.6.1.5.5.7.3.1" rev_data test_eku_all.crt test_eku_allbutcodesigningEE.crt
test_ekuchecker "Test-EKU-clientAuth" ENE "1.3.6.1.5.5.7.3.2" rev_data test_eku_all.crt test_eku_allbutcodesigningEE.crt
test_ekuchecker "Test-EKU-codesigning-without-OID" EE "1.3.6.1.5.5.7.3.3" rev_data test_eku_all.crt test_eku_allbutcodesigningEE.crt
test_ekuchecker "Test-EKU-emailProtection" ENE "1.3.6.1.5.5.7.3.4" rev_data test_eku_all.crt test_eku_allbutcodesigningEE.crt
test_ekuchecker "Test-EKU-timestamping" ENE "1.3.6.1.5.5.7.3.8" rev_data test_eku_all.crt test_eku_allbutcodesigningEE.crt
test_ekuchecker "Test-EKU-OCSPSigning" ENE "1.3.6.1.5.5.7.3.9" rev_data test_eku_all.crt test_eku_allbutcodesigningEE.crt
test_ekuchecker "Test-EKU-only-EE-serverAuth" ENE "E1.3.6.1.5.5.7.3.1" rev_data test_eku_all.crt test_eku_allbutcodesigningEE.crt
test_ekuchecker "Test-EKU-only-EE-clientAuth" ENE "E1.3.6.1.5.5.7.3.2" rev_data test_eku_all.crt test_eku_allbutcodesigningEE.crt
test_ekuchecker "Test-EKU-only-EE-codesigning-without-OID" EE "E1.3.6.1.5.5.7.3.3" rev_data test_eku_all.crt test_eku_allbutcodesigningEE.crt
test_ekuchecker "Test-EKU-only-EE-emailProtection" ENE "E1.3.6.1.5.5.7.3.4" rev_data test_eku_all.crt test_eku_allbutcodesigningEE.crt
test_ekuchecker "Test-EKU-only-EE-timestamping" ENE "E1.3.6.1.5.5.7.3.8" rev_data test_eku_all.crt test_eku_allbutcodesigningEE.crt
test_ekuchecker "Test-EKU-only-EE-ocspSigning" ENE "E1.3.6.1.5.5.7.3.9" rev_data test_eku_all.crt test_eku_allbutcodesigningEE.crt
test_socket ${HOSTADDR}:2000
EOF

totalErrors=$?
html_msg ${totalErrors} 0 "&nbsp;&nbsp;&nbsp;${testunit}: passed ${passed} of ${numtests} tests"
exit ${totalErrors}

