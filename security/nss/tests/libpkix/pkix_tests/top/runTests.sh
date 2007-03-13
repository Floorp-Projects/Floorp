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


LDAP='nss.red.iplanet.com:1389'
export LDAP
curdir=`pwd`
cd ../../common
. ./libpkix_init.sh > /dev/null
doPD=1
doOCSP=1
. ./libpkix_init_nist.sh
cd ${curdir}

numtests=0
passed=0
testunit=TOP
doTop=1
linkMStoreNistFiles="store1/TrustAnchorRootCRL.crl
    store1/TwoCRLsCABadCRL.crl
    store2/TwoCRLsCAGoodCRL.crl"

if [ ! -z "${NIST_FILES_DIR}" ] ; then
    if [ -d ${HOSTDIR}/rev_data/multiple_certstores ]; then
        rm -fr ${HOSTDIR}/rev_data/multiple_certstores
    fi
    mkdir -p ${HOSTDIR}/rev_data/multiple_certstores
    mkdir -p ${HOSTDIR}/rev_data/multiple_certstores/store1
    mkdir -p ${HOSTDIR}/rev_data/multiple_certstores/store2
    for i in ${linkMStoreNistFiles}; do
        if [ -f ${HOSTDIR}/rev_data/multiple_certstores/$i ]; then
            rm ${HOSTDIR}/rev_data/multiple_certstores/$i
        fi
        fname=`basename $i`
        cp ${NIST_FILES_DIR}/${fname} ${HOSTDIR}/rev_data/multiple_certstores/$i
    done
fi

ocspFiles="goodcert.crt revokedcert.crt anchorcert.crt
    secmod.db key3.db cert8.db"

if [ ! -z ${doOCSPTest} ] ; then
    if [ -d ${HOSTDIR}/ocsp ]; then
        rm -fr ${HOSTDIR}/ocsp
    fi
    mkdir -p ${HOSTDIR}/ocsp
    for i in ${ocspFiles}; do
        cp $i ${HOSTDIR}/ocsp/$i

    done
fi

##########
# main
##########

ParseArgs $*

Display ""
Display "#    ENE = expect no error (validation should succeed)"
Display "#    EE = expect error (validation should fail)"
Display ""

LOGGING=1
SOCKETTRACE=1
export LOGGING SOCKETTRACE

RunTests <<EOF
test_validatechain_NB NIST-Test.4.1.1 ENE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt ValidCertificatePathTest1EE.crt
test_validatechain_NB NIST-Test.4.1.2 EE $NIST TrustAnchorRootCertificate.crt BadSignedCACert.crt InvalidCASignatureTest2EE.crt
test_validatechain_NB NIST-Test.4.1.3 EE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt  InvalidEESignatureTest3EE.crt
test_validatechain_NB NIST-Test.4.1.4 ENE $NIST TrustAnchorRootCertificate.crt DSACACert.crt ValidDSASignaturesTest4EE.crt
test_validatechain_NB NIST-Test.4.1.5 ENE $NIST TrustAnchorRootCertificate.crt DSACACert.crt DSAParametersInheritedCACert.crt ValidDSAParameterInheritanceTest5EE.crt
EOF

tracedErrors=$?

LOGGING=0
SOCKETTRACE=0

RunTests <<EOF
test_basicchecker ../../certs
test_basicconstraintschecker "Two-Certificates-Chain" ENE ../../certs hy2hy-bc0 hy2hc-bc
test_basicconstraintschecker "Three-Certificates-Chain" ENE ../../certs hy2hy-bc0 hy2hy-bc0 hy2hc-bc
test_basicconstraintschecker "Four-Certificates-Chain-with-error" EE ../../certs hy2hy-bc0 hy2hy-bc0 hy2hc-bc hy2hc-bc
test_validatechain_bc  ../../certs/hy2hy-bc0 ../../certs/hy2hc-bc
test_policychecker NIST-Test-Files-Used ENE $NIST ../../certs
test_defaultcrlchecker2stores NIST-Test.4.4.7-with-multiple-CRL-stores ENE $NIST ${HOSTDIR}/rev_data/multiple_certstores/store1 ${HOSTDIR}/rev_data/multiple_certstores/store2 TrustAnchorRootCertificate.crt TwoCRLsCACert.crt ValidTwoCRLsTest7EE.crt
test_buildchain_resourcelimits ${LDAP} NIST-Test.4.5.1 ENE $NIST ValidBasicSelfIssuedOldWithNewTest1EE.crt BasicSelfIssuedNewKeyOldWithNewCACert.crt BasicSelfIssuedNewKeyCACert.crt TrustAnchorRootCertificate.crt
test_customcrlchecker "CRL-test-without-revocation" ENE rev_data/crlchecker sci2sci.crt sci2phy.crt phy2prof.crt prof2test.crt
test_customcrlchecker "CRL-test-with-revocation-reasoncode" EE rev_data/crlchecker sci2sci.crt sci2chem.crt chem2prof.crt prof2test.crt
test_subjaltnamechecker "NIST-Test-Files-Used" "0R:testcertificates.gov+R:Test23EE@testcertificates.gov" ENE $NIST TrustAnchorRootCertificate.crt nameConstraintsRFC822CA2Cert.crt ValidRFC822nameConstraintsTest23EE.crt
test_subjaltnamechecker "NIST-Test-Files-Used" "0R:TEST.gov" EE $NIST TrustAnchorRootCertificate.crt nameConstraintsRFC822CA2Cert.crt ValidRFC822nameConstraintsTest23EE.crt
test_subjaltnamechecker "NIST-Test-Files-Used" "0N:testcertificates.gov+N:testserver.testcertificates.gov" ENE $NIST TrustAnchorRootCertificate.crt nameConstraintsDNS1CACert.crt ValidDNSnameConstraintsTest30EE.crt
test_subjaltnamechecker "NIST-Test-Files-Used" "0N:notestcertificates.gov" EE $NIST TrustAnchorRootCertificate.crt nameConstraintsDNS1CACert.crt ValidDNSnameConstraintsTest30EE.crt
test_subjaltnamechecker "NIST-Test-Files-Used" "0U:.gov+U:http://testserver.testcertificates.gov/index.html" ENE $NIST TrustAnchorRootCertificate.crt nameConstraintsURI1CACert.crt ValidURInameConstraintsTest34EE.crt
test_subjaltnamechecker "NIST-Test-Files-Used" "0U:test.testcertificates.gov" EE $NIST TrustAnchorRootCertificate.crt nameConstraintsURI1CACert.crt ValidURInameConstraintsTest34EE.crt
test_subjaltnamechecker "NIST-Test-Files-Used" "1D:C=US+D:CN=Certificates,C=US" EE $NIST TrustAnchorRootCertificate.crt nameConstraintsDN2CACert.crt ValidDNnameConstraintsTest5EE.crt
test_subjaltnamechecker "NIST-Test-Files-Used" "0D:O=TestCertificates,C=CN" EE $NIST TrustAnchorRootCertificate.crt nameConstraintsDN2CACert.crt ValidDNnameConstraintsTest5EE.crt
test_validatechain "CRL-test-without-key-usage-cRLsign-bit-NIST-Test-Files-Used" EE $NIST TrustAnchorRootCertificate.crt SeparateCertificateandCRLKeysCertificateSigningCACert.crt SeparateCertificateandCRLKeysCRLSigningCert.crt InvalidSeparateCertificateandCRLKeysTest20EE.crt
test_validatechain NIST-Test.4.1.1 ENE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt ValidCertificatePathTest1EE.crt
test_validatechain NIST-Test.4.1.2 EE $NIST TrustAnchorRootCertificate.crt BadSignedCACert.crt InvalidCASignatureTest2EE.crt
test_validatechain NIST-Test.4.1.3 EE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt  InvalidEESignatureTest3EE.crt
test_validatechain NIST-Test.4.1.4 ENE $NIST TrustAnchorRootCertificate.crt DSACACert.crt ValidDSASignaturesTest4EE.crt
test_validatechain NIST-Test.4.1.5 ENE $NIST TrustAnchorRootCertificate.crt DSACACert.crt DSAParametersInheritedCACert.crt ValidDSAParameterInheritanceTest5EE.crt
test_validatechain NIST-Test.4.1.6 EE $NIST TrustAnchorRootCertificate.crt DSACACert.crt InvalidDSASignatureTest6EE.crt
test_validatechain NIST-Test.4.2.1 EE $NIST TrustAnchorRootCertificate.crt BadnotBeforeDateCACert.crt InvalidCAnotBeforeDateTest1EE.crt
test_validatechain NIST-Test.4.2.2 EE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt InvalidEEnotBeforeDateTest2EE.crt
test_validatechain NIST-Test.4.2.3 ENE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt Validpre2000UTCnotBeforeDateTest3EE.crt
test_validatechain NIST-Test.4.2.4 ENE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt ValidGeneralizedTimenotBeforeDateTest4EE.crt
test_validatechain NIST-Test.4.2.5 EE $NIST TrustAnchorRootCertificate.crt BadnotAfterDateCACert.crt InvalidCAnotAfterDateTest5EE.crt
test_validatechain NIST-Test.4.2.6 EE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt InvalidEEnotAfterDateTest6EE.crt
test_validatechain NIST-Test.4.2.7 EE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt Invalidpre2000UTCEEnotAfterDateTest7EE.crt
test_validatechain NIST-Test.4.2.8 ENE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt ValidGeneralizedTimenotAfterDateTest8EE.crt
test_validatechain NIST-Test.4.3.1 EE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt InvalidNameChainingTest1EE.crt
test_validatechain NIST-Test.4.3.2 EE $NIST TrustAnchorRootCertificate.crt NameOrderingCACert.crt  InvalidNameChainingOrderTest2EE.crt
test_validatechain NIST-Test.4.3.3 ENE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt ValidNameChainingWhitespaceTest3EE.crt
test_validatechain NIST-Test.4.3.4 ENE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt ValidNameChainingWhitespaceTest4EE.crt
test_validatechain NIST-Test.4.3.5 ENE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt ValidNameChainingCapitalizationTest5EE.crt
test_validatechain NIST-Test.4.3.6 ENE $NIST TrustAnchorRootCertificate.crt UIDCACert.crt  ValidNameUIDsTest6EE.crt
test_validatechain NIST-Test.4.3.9 ENE $NIST TrustAnchorRootCertificate.crt UTF8StringEncodedNamesCACert.crt  ValidUTF8StringEncodedNamesTest9EE.crt
test_validatechain NIST-Test.4.3.10 ENE $NIST TrustAnchorRootCertificate.crt RolloverfromPrintableStringtoUTF8StringCACert.crt  ValidRolloverfromPrintableStringtoUTF8StringTest10EE.crt
test_validatechain NIST-Test.4.3.11 ENE $NIST TrustAnchorRootCertificate.crt UTF8StringCaseInsensitiveMatchCACert.crt  ValidUTF8StringCaseInsensitiveMatchTest11EE.crt
test_validatechain NIST-Test.4.4.1 EE $NIST TrustAnchorRootCertificate.crt NoCRLCACert.crt InvalidMissingCRLTest1EE.crt
test_validatechain NIST-Test.4.4.2 EE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt RevokedsubCACert.crt InvalidRevokedCATest2EE.crt
test_validatechain NIST-Test.4.4.3 EE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt InvalidRevokedEETest3EE.crt
test_validatechain NIST-Test.4.4.4 EE $NIST TrustAnchorRootCertificate.crt BadCRLSignatureCACert.crt InvalidBadCRLSignatureTest4EE.crt
test_validatechain NIST-Test.4.4.5 EE $NIST TrustAnchorRootCertificate.crt BadCRLIssuerNameCACert.crt InvalidBadCRLIssuerNameTest5EE.crt
test_validatechain NIST-Test.4.4.6 EE $NIST TrustAnchorRootCertificate.crt WrongCRLCACert.crt InvalidWrongCRLTest6EE.crt
test_validatechain NIST-Test.4.4.7 ENE $NIST TrustAnchorRootCertificate.crt TwoCRLsCACert.crt ValidTwoCRLsTest7EE.crt
test_validatechain NIST-Test.4.4.8 EE $NIST TrustAnchorRootCertificate.crt UnknownCRLEntryExtensionCACert.crt InvalidUnknownCRLEntryExtensionTest8EE.crt
test_validatechain NIST-Test.4.4.9 EE $NIST TrustAnchorRootCertificate.crt UnknownCRLExtensionCACert.crt InvalidUnknownCRLExtensionTest9EE.crt
test_validatechain NIST-Test.4.4.10 EE $NIST TrustAnchorRootCertificate.crt UnknownCRLExtensionCACert.crt InvalidUnknownCRLExtensionTest10EE.crt
test_validatechain NIST-Test.4.4.11 EE $NIST TrustAnchorRootCertificate.crt OldCRLnextUpdateCACert.crt InvalidOldCRLnextUpdateTest11EE.crt
test_validatechain NIST-Test.4.4.12 EE $NIST TrustAnchorRootCertificate.crt pre2000CRLnextUpdateCACert.crt Invalidpre2000CRLnextUpdateTest12EE.crt
test_validatechain NIST-Test.4.4.13 ENE $NIST TrustAnchorRootCertificate.crt GeneralizedTimeCRLnextUpdateCACert.crt ValidGeneralizedTimeCRLnextUpdateTest13EE.crt
test_validatechain NIST-Test.4.4.14 ENE $NIST TrustAnchorRootCertificate.crt NegativeSerialNumberCACert.crt ValidNegativeSerialNumberTest14EE.crt
test_validatechain NIST-Test.4.4.15 EE $NIST TrustAnchorRootCertificate.crt NegativeSerialNumberCACert.crt InvalidNegativeSerialNumberTest15EE.crt
test_validatechain NIST-Test.4.4.16 ENE $NIST TrustAnchorRootCertificate.crt LongSerialNumberCACert.crt ValidLongSerialNumberTest16EE.crt
test_validatechain NIST-Test.4.4.17 ENE $NIST TrustAnchorRootCertificate.crt LongSerialNumberCACert.crt ValidLongSerialNumberTest17EE.crt
test_validatechain NIST-Test.4.4.18 EE $NIST TrustAnchorRootCertificate.crt LongSerialNumberCACert.crt InvalidLongSerialNumberTest18EE.crt
test_validatechain NIST-Test.4.4.20 EE $NIST TrustAnchorRootCertificate.crt SeparateCertificateandCRLKeysCertificateSigningCACert.crt SeparateCertificateandCRLKeysCRLSigningCert.crt InvalidSeparateCertificateandCRLKeysTest20EE.crt
test_validatechain NIST-Test.4.5.1 ENE $NIST TrustAnchorRootCertificate.crt BasicSelfIssuedNewKeyCACert.crt BasicSelfIssuedNewKeyOldWithNewCACert.crt ValidBasicSelfIssuedOldWithNewTest1EE.crt
test_validatechain NIST-Test.4.5.2 EE $NIST TrustAnchorRootCertificate.crt BasicSelfIssuedNewKeyCACert.crt BasicSelfIssuedNewKeyOldWithNewCACert.crt InvalidBasicSelfIssuedOldWithNewTest2EE.crt
test_validatechain NIST-Test.4.5.5 EE $NIST TrustAnchorRootCertificate.crt BasicSelfIssuedOldKeyCACert.crt BasicSelfIssuedOldKeyNewWithOldCACert.crt InvalidBasicSelfIssuedNewWithOldTest5EE.crt
test_validatechain NIST-Test.4.5.7 EE $NIST TrustAnchorRootCertificate.crt BasicSelfIssuedCRLSigningKeyCACert.crt BasicSelfIssuedCRLSigningKeyCRLCert.crt InvalidBasicSelfIssuedCRLSigningKeyTest7EE.crt
test_validatechain NIST-Test.4.5.8 EE $NIST TrustAnchorRootCertificate.crt BasicSelfIssuedCRLSigningKeyCACert.crt BasicSelfIssuedCRLSigningKeyCRLCert.crt InvalidBasicSelfIssuedCRLSigningKeyTest8EE.crt
test_validatechain_NB "CRL-test-without-key-usage-cRLsign-bit-NIST-Test-Files-Used" EE $NIST TrustAnchorRootCertificate.crt SeparateCertificateandCRLKeysCertificateSigningCACert.crt SeparateCertificateandCRLKeysCRLSigningCert.crt InvalidSeparateCertificateandCRLKeysTest20EE.crt
test_validatechain_NB NIST-Test.4.1.1 ENE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt ValidCertificatePathTest1EE.crt
test_validatechain_NB NIST-Test.4.1.2 EE $NIST TrustAnchorRootCertificate.crt BadSignedCACert.crt InvalidCASignatureTest2EE.crt
test_validatechain_NB NIST-Test.4.1.3 EE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt  InvalidEESignatureTest3EE.crt
test_validatechain_NB NIST-Test.4.1.4 ENE $NIST TrustAnchorRootCertificate.crt DSACACert.crt ValidDSASignaturesTest4EE.crt
test_validatechain_NB NIST-Test.4.1.5 ENE $NIST TrustAnchorRootCertificate.crt DSACACert.crt DSAParametersInheritedCACert.crt ValidDSAParameterInheritanceTest5EE.crt
test_validatechain_NB NIST-Test.4.1.6 EE $NIST TrustAnchorRootCertificate.crt DSACACert.crt InvalidDSASignatureTest6EE.crt
test_validatechain_NB NIST-Test.4.2.1 EE $NIST TrustAnchorRootCertificate.crt BadnotBeforeDateCACert.crt InvalidCAnotBeforeDateTest1EE.crt
test_validatechain_NB NIST-Test.4.2.2 EE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt InvalidEEnotBeforeDateTest2EE.crt
test_validatechain_NB NIST-Test.4.2.3 ENE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt Validpre2000UTCnotBeforeDateTest3EE.crt
test_validatechain_NB NIST-Test.4.2.4 ENE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt ValidGeneralizedTimenotBeforeDateTest4EE.crt
test_validatechain_NB NIST-Test.4.2.5 EE $NIST TrustAnchorRootCertificate.crt BadnotAfterDateCACert.crt InvalidCAnotAfterDateTest5EE.crt
test_validatechain_NB NIST-Test.4.2.6 EE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt InvalidEEnotAfterDateTest6EE.crt
test_validatechain_NB NIST-Test.4.2.7 EE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt Invalidpre2000UTCEEnotAfterDateTest7EE.crt
test_validatechain_NB NIST-Test.4.2.8 ENE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt ValidGeneralizedTimenotAfterDateTest8EE.crt
test_validatechain_NB NIST-Test.4.3.1 EE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt InvalidNameChainingTest1EE.crt
test_validatechain_NB NIST-Test.4.3.2 EE $NIST TrustAnchorRootCertificate.crt NameOrderingCACert.crt  InvalidNameChainingOrderTest2EE.crt
test_validatechain_NB NIST-Test.4.3.3 ENE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt ValidNameChainingWhitespaceTest3EE.crt
test_validatechain_NB NIST-Test.4.3.4 ENE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt ValidNameChainingWhitespaceTest4EE.crt
test_validatechain_NB NIST-Test.4.3.5 ENE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt ValidNameChainingCapitalizationTest5EE.crt
test_validatechain_NB NIST-Test.4.3.6 ENE $NIST TrustAnchorRootCertificate.crt UIDCACert.crt  ValidNameUIDsTest6EE.crt
test_validatechain_NB NIST-Test.4.3.9 ENE $NIST TrustAnchorRootCertificate.crt UTF8StringEncodedNamesCACert.crt  ValidUTF8StringEncodedNamesTest9EE.crt
test_validatechain_NB NIST-Test.4.3.10 ENE $NIST TrustAnchorRootCertificate.crt RolloverfromPrintableStringtoUTF8StringCACert.crt  ValidRolloverfromPrintableStringtoUTF8StringTest10EE.crt
test_validatechain_NB NIST-Test.4.3.11 ENE $NIST TrustAnchorRootCertificate.crt UTF8StringCaseInsensitiveMatchCACert.crt  ValidUTF8StringCaseInsensitiveMatchTest11EE.crt
test_validatechain_NB NIST-Test.4.4.1 EE $NIST TrustAnchorRootCertificate.crt NoCRLCACert.crt InvalidMissingCRLTest1EE.crt
test_validatechain_NB NIST-Test.4.4.2 EE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt RevokedsubCACert.crt InvalidRevokedCATest2EE.crt
test_validatechain_NB NIST-Test.4.4.3 EE $NIST TrustAnchorRootCertificate.crt GoodCACert.crt InvalidRevokedEETest3EE.crt
test_validatechain_NB NIST-Test.4.4.4 EE $NIST TrustAnchorRootCertificate.crt BadCRLSignatureCACert.crt InvalidBadCRLSignatureTest4EE.crt
test_validatechain_NB NIST-Test.4.4.5 EE $NIST TrustAnchorRootCertificate.crt BadCRLIssuerNameCACert.crt InvalidBadCRLIssuerNameTest5EE.crt
test_validatechain_NB NIST-Test.4.4.6 EE $NIST TrustAnchorRootCertificate.crt WrongCRLCACert.crt InvalidWrongCRLTest6EE.crt
test_validatechain_NB NIST-Test.4.4.7 ENE $NIST TrustAnchorRootCertificate.crt TwoCRLsCACert.crt ValidTwoCRLsTest7EE.crt
test_validatechain_NB NIST-Test.4.4.8 EE $NIST TrustAnchorRootCertificate.crt UnknownCRLEntryExtensionCACert.crt InvalidUnknownCRLEntryExtensionTest8EE.crt
test_validatechain_NB NIST-Test.4.4.9 EE $NIST TrustAnchorRootCertificate.crt UnknownCRLExtensionCACert.crt InvalidUnknownCRLExtensionTest9EE.crt
test_validatechain_NB NIST-Test.4.4.10 EE $NIST TrustAnchorRootCertificate.crt UnknownCRLExtensionCACert.crt InvalidUnknownCRLExtensionTest10EE.crt
test_validatechain_NB NIST-Test.4.4.11 EE $NIST TrustAnchorRootCertificate.crt OldCRLnextUpdateCACert.crt InvalidOldCRLnextUpdateTest11EE.crt
test_validatechain_NB NIST-Test.4.4.12 EE $NIST TrustAnchorRootCertificate.crt pre2000CRLnextUpdateCACert.crt Invalidpre2000CRLnextUpdateTest12EE.crt
test_validatechain_NB NIST-Test.4.4.13 ENE $NIST TrustAnchorRootCertificate.crt GeneralizedTimeCRLnextUpdateCACert.crt ValidGeneralizedTimeCRLnextUpdateTest13EE.crt
test_validatechain_NB NIST-Test.4.4.14 ENE $NIST TrustAnchorRootCertificate.crt NegativeSerialNumberCACert.crt ValidNegativeSerialNumberTest14EE.crt
test_validatechain_NB NIST-Test.4.4.15 EE $NIST TrustAnchorRootCertificate.crt NegativeSerialNumberCACert.crt InvalidNegativeSerialNumberTest15EE.crt
test_validatechain_NB NIST-Test.4.4.16 ENE $NIST TrustAnchorRootCertificate.crt LongSerialNumberCACert.crt ValidLongSerialNumberTest16EE.crt
test_validatechain_NB NIST-Test.4.4.17 ENE $NIST TrustAnchorRootCertificate.crt LongSerialNumberCACert.crt ValidLongSerialNumberTest17EE.crt
test_validatechain_NB NIST-Test.4.4.18 EE $NIST TrustAnchorRootCertificate.crt LongSerialNumberCACert.crt InvalidLongSerialNumberTest18EE.crt
test_validatechain_NB NIST-Test.4.4.20 EE $NIST TrustAnchorRootCertificate.crt SeparateCertificateandCRLKeysCertificateSigningCACert.crt SeparateCertificateandCRLKeysCRLSigningCert.crt InvalidSeparateCertificateandCRLKeysTest20EE.crt
test_validatechain_NB NIST-Test.4.5.1 ENE $NIST TrustAnchorRootCertificate.crt BasicSelfIssuedNewKeyCACert.crt BasicSelfIssuedNewKeyOldWithNewCACert.crt ValidBasicSelfIssuedOldWithNewTest1EE.crt
test_validatechain_NB NIST-Test.4.5.2 EE $NIST TrustAnchorRootCertificate.crt BasicSelfIssuedNewKeyCACert.crt BasicSelfIssuedNewKeyOldWithNewCACert.crt InvalidBasicSelfIssuedOldWithNewTest2EE.crt
test_validatechain_NB NIST-Test.4.5.5 EE $NIST TrustAnchorRootCertificate.crt BasicSelfIssuedOldKeyCACert.crt BasicSelfIssuedOldKeyNewWithOldCACert.crt InvalidBasicSelfIssuedNewWithOldTest5EE.crt
test_validatechain_NB NIST-Test.4.5.7 EE $NIST TrustAnchorRootCertificate.crt BasicSelfIssuedCRLSigningKeyCACert.crt BasicSelfIssuedCRLSigningKeyCRLCert.crt InvalidBasicSelfIssuedCRLSigningKeyTest7EE.crt
test_validatechain_NB NIST-Test.4.5.8 EE $NIST TrustAnchorRootCertificate.crt BasicSelfIssuedCRLSigningKeyCACert.crt BasicSelfIssuedCRLSigningKeyCRLCert.crt InvalidBasicSelfIssuedCRLSigningKeyTest8EE.crt
test_basicconstraintschecker NIST-Test.4.6.1 EE $NIST TrustAnchorRootCertificate.crt MissingbasicConstraintsCACert.crt InvalidMissingbasicConstraintsTest1EE.crt
test_basicconstraintschecker NIST-Test.4.6.2 EE $NIST TrustAnchorRootCertificate.crt basicConstraintsCriticalcAFalseCACert.crt InvalidcAFalseTest2EE.crt
test_basicconstraintschecker NIST-Test.4.6.3 EE $NIST TrustAnchorRootCertificate.crt basicConstraintsNotCriticalcAFalseCACert.crt InvalidcAFalseTest3EE.crt
test_basicconstraintschecker NIST-Test.4.6.4 ENE $NIST TrustAnchorRootCertificate.crt basicConstraintsNotCriticalCACert.crt ValidbasicConstraintsNotCriticalTest4EE.crt
test_basicconstraintschecker NIST-Test.4.6.5 EE $NIST TrustAnchorRootCertificate.crt pathLenConstraint0CACert.crt pathLenConstraint0subCACert.crt InvalidpathLenConstraintTest5EE.crt
test_basicconstraintschecker NIST-Test.4.6.6 EE $NIST TrustAnchorRootCertificate.crt pathLenConstraint0CACert.crt pathLenConstraint0subCACert.crt InvalidpathLenConstraintTest6EE.crt
test_basicconstraintschecker NIST-Test.4.6.7 ENE $NIST TrustAnchorRootCertificate.crt pathLenConstraint0CACert.crt ValidpathLenConstraintTest7EE.crt
test_basicconstraintschecker NIST-Test.4.6.8 ENE $NIST TrustAnchorRootCertificate.crt pathLenConstraint0CACert.crt ValidpathLenConstraintTest8EE.crt
test_basicconstraintschecker NIST-Test.4.6.9 EE $NIST TrustAnchorRootCertificate.crt pathLenConstraint6CACert.crt pathLenConstraint6subCA0Cert.crt pathLenConstraint6subsubCA00Cert.crt InvalidpathLenConstraintTest9EE.crt
test_basicconstraintschecker NIST-Test.4.6.10 EE $NIST TrustAnchorRootCertificate.crt pathLenConstraint6CACert.crt pathLenConstraint6subCA0Cert.crt pathLenConstraint6subsubCA00Cert.crt InvalidpathLenConstraintTest10EE.crt
test_basicconstraintschecker NIST-Test.4.6.11 EE $NIST TrustAnchorRootCertificate.crt pathLenConstraint6CACert.crt pathLenConstraint6subCA1Cert.crt pathLenConstraint6subsubCA11Cert.crt pathLenConstraint6subsubsubCA11XCert.crt InvalidpathLenConstraintTest11EE.crt
test_basicconstraintschecker NIST-Test.4.6.12 EE $NIST TrustAnchorRootCertificate.crt pathLenConstraint6CACert.crt pathLenConstraint6subCA1Cert.crt pathLenConstraint6subsubCA11Cert.crt pathLenConstraint6subsubsubCA11XCert.crt InvalidpathLenConstraintTest12EE.crt
test_basicconstraintschecker NIST-Test.4.6.13 ENE $NIST TrustAnchorRootCertificate.crt pathLenConstraint6CACert.crt pathLenConstraint6subCA4Cert.crt pathLenConstraint6subsubCA41Cert.crt pathLenConstraint6subsubsubCA41XCert.crt ValidpathLenConstraintTest13EE.crt
test_basicconstraintschecker NIST-Test.4.6.14 ENE $NIST TrustAnchorRootCertificate.crt pathLenConstraint6CACert.crt pathLenConstraint6subCA4Cert.crt pathLenConstraint6subsubCA41Cert.crt pathLenConstraint6subsubsubCA41XCert.crt ValidpathLenConstraintTest14EE.crt
test_basicconstraintschecker NIST-Test.4.6.15 ENE $NIST TrustAnchorRootCertificate.crt pathLenConstraint0CACert.crt pathLenConstraint0SelfIssuedCACert.crt ValidSelfIssuedpathLenConstraintTest15EE.crt
test_basicconstraintschecker NIST-Test.4.6.16 EE $NIST TrustAnchorRootCertificate.crt pathLenConstraint0CACert.crt pathLenConstraint0SelfIssuedCACert.crt pathLenConstraint0subCA2Cert.crt InvalidSelfIssuedpathLenConstraintTest16EE.crt
test_basicconstraintschecker NIST-Test.4.6.17 ENE $NIST TrustAnchorRootCertificate.crt pathLenConstraint1CACert.crt pathLenConstraint1SelfIssuedCACert.crt pathLenConstraint1subCACert.crt pathLenConstraint1SelfIssuedsubCACert.crt ValidSelfIssuedpathLenConstraintTest17EE.crt
test_validatechain "NIST-Test.4.7.1" EE $NIST TrustAnchorRootCertificate.crt keyUsageCriticalkeyCertSignFalseCACert.crt InvalidkeyUsageCriticalkeyCertSignFalseTest1EE.crt
test_validatechain "NIST-Test.4.7.2" EE $NIST TrustAnchorRootCertificate.crt keyUsageNotCriticalkeyCertSignFalseCACert.crt InvalidkeyUsageNotCriticalkeyCertSignFalseTest2EE.crt
test_validatechain "NIST-Test.4.7.3" ENE $NIST TrustAnchorRootCertificate.crt keyUsageNotCriticalCACert.crt ValidkeyUsageNotCriticalTest3EE.crt
test_validatechain "NIST-Test.4.7.4" EE $NIST TrustAnchorRootCertificate.crt keyUsageCriticalcRLSignFalseCACert.crt InvalidkeyUsageCriticalcRLSignFalseTest4EE.crt
test_validatechain "NIST-Test.4.7.5" EE $NIST TrustAnchorRootCertificate.crt  keyUsageNotCriticalcRLSignFalseCACert.crt InvalidkeyUsageNotCriticalcRLSignFalseTest5EE.crt
test_policychecker NIST-Test.4.8.1.1-1 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt GoodCACert.crt ValidCertificatePathTest1EE.crt
test_policychecker NIST-Test.4.8.1.1-2 ENE $NIST ../../certs "{2.5.29.32.0}" E TrustAnchorRootCertificate.crt GoodCACert.crt ValidCertificatePathTest1EE.crt
test_policychecker NIST-Test.4.8.1.2 ENE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.1}" E TrustAnchorRootCertificate.crt GoodCACert.crt ValidCertificatePathTest1EE.crt
test_policychecker NIST-Test.4.8.1.3 EE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.2}" E TrustAnchorRootCertificate.crt GoodCACert.crt ValidCertificatePathTest1EE.crt
test_policychecker NIST-Test.4.8.1.4 ENE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.1:2.16.840.1.101.3.2.1.48.2}" E TrustAnchorRootCertificate.crt GoodCACert.crt ValidCertificatePathTest1EE.crt
test_policychecker NIST-Test.4.8.2.1 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt NoPoliciesCACert.crt AllCertificatesNoPoliciesTest2EE.crt
test_policychecker NIST-Test.4.8.2.2 EE $NIST ../../certs "{2.5.29.32.0}" E TrustAnchorRootCertificate.crt NoPoliciesCACert.crt AllCertificatesNoPoliciesTest2EE.crt
test_policychecker NIST-Test.4.8.3.1 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt GoodCACert.crt PoliciesP2subCACert.crt DifferentPoliciesTest3EE.crt
test_policychecker NIST-Test.4.8.3.2 EE $NIST ../../certs "{2.5.29.32.0}" E TrustAnchorRootCertificate.crt GoodCACert.crt PoliciesP2subCACert.crt DifferentPoliciesTest3EE.crt
test_policychecker NIST-Test.4.8.3.3 EE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.1:2.16.840.1.101.3.2.1.48.2}" E TrustAnchorRootCertificate.crt GoodCACert.crt PoliciesP2subCACert.crt DifferentPoliciesTest3EE.crt
test_policychecker NIST-Test.4.8.4 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt GoodCACert.crt GoodsubCACert.crt DifferentPoliciesTest4EE.crt
test_policychecker NIST-Test.4.8.5 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt GoodCACert.crt PoliciesP2subCA2Cert.crt DifferentPoliciesTest5EE.crt
test_policychecker NIST-Test.4.8.6.1 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt PoliciesP1234CACert.crt PoliciesP1234subCAP123Cert.crt PoliciesP1234subsubCAP123P12Cert.crt OverlappingPoliciesTest6EE.crt
test_policychecker NIST-Test.4.8.6.2 ENE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.1}" TrustAnchorRootCertificate.crt PoliciesP1234CACert.crt PoliciesP1234subCAP123Cert.crt PoliciesP1234subsubCAP123P12Cert.crt OverlappingPoliciesTest6EE.crt
test_policychecker NIST-Test.4.8.6.3 EE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.2}" TrustAnchorRootCertificate.crt PoliciesP1234CACert.crt PoliciesP1234subCAP123Cert.crt PoliciesP1234subsubCAP123P12Cert.crt OverlappingPoliciesTest6EE.crt
test_policychecker NIST-Test.4.8.7 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt PoliciesP123CACert.crt PoliciesP123subCAP12Cert.crt PoliciesP123subsubCAP12P1Cert.crt DifferentPoliciesTest7EE.crt
test_policychecker NIST-Test.4.8.8 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt PoliciesP12CACert.crt PoliciesP12subCAP1Cert.crt PoliciesP12subsubCAP1P2Cert.crt DifferentPoliciesTest8EE.crt
test_policychecker NIST-Test.4.8.9 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt PoliciesP123CACert.crt PoliciesP123subCAP12Cert.crt PoliciesP123subsubCAP12P2Cert.crt PoliciesP123subsubsubCAP12P2P1Cert.crt
test_policychecker NIST-Test.4.8.10.1 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt PoliciesP12CACert.crt AllCertificatesSamePoliciesTest10EE.crt
test_policychecker NIST-Test.4.8.10.2 ENE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.1}" TrustAnchorRootCertificate.crt PoliciesP12CACert.crt AllCertificatesSamePoliciesTest10EE.crt
test_policychecker NIST-Test.4.8.10.3 ENE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.2}" TrustAnchorRootCertificate.crt PoliciesP12CACert.crt AllCertificatesSamePoliciesTest10EE.crt
test_policychecker NIST-Test.4.8.11.1 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt anyPolicyCACert.crt AllCertificatesanyPolicyTest11EE.crt
test_policychecker NIST-Test.4.8.11.2 ENE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.1}" TrustAnchorRootCertificate.crt anyPolicyCACert.crt AllCertificatesanyPolicyTest11EE.crt
test_policychecker NIST-Test.4.8.12 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt PoliciesP3CACert.crt DifferentPoliciesTest12EE.crt
test_policychecker NIST-Test.4.8.13.1 ENE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.1}" TrustAnchorRootCertificate.crt PoliciesP123CACert.crt AllCertificatesSamePoliciesTest13EE.crt
test_policychecker NIST-Test.4.8.13.2 ENE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.2}" TrustAnchorRootCertificate.crt PoliciesP123CACert.crt AllCertificatesSamePoliciesTest13EE.crt
test_policychecker NIST-Test.4.8.13.3 ENE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.3}" TrustAnchorRootCertificate.crt PoliciesP123CACert.crt AllCertificatesSamePoliciesTest13EE.crt
test_policychecker NIST-Test.4.8.14.1 ENE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.1}" TrustAnchorRootCertificate.crt anyPolicyCACert.crt AnyPolicyTest14EE.crt
test_policychecker NIST-Test.4.8.14.2 EE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.2}" E TrustAnchorRootCertificate.crt anyPolicyCACert.crt AnyPolicyTest14EE.crt
test_policychecker NIST-Test.4.8.15.1 ENE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.1}" E TrustAnchorRootCertificate.crt UserNoticeQualifierTest15EE.crt
test_policychecker NIST-Test.4.8.15.2 EE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.2}" E TrustAnchorRootCertificate.crt UserNoticeQualifierTest15EE.crt
test_policychecker NIST-Test.4.8.16.1 ENE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.1}" E TrustAnchorRootCertificate.crt GoodCACert.crt UserNoticeQualifierTest16EE.crt
test_policychecker NIST-Test.4.8.16.2 EE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.2}" E TrustAnchorRootCertificate.crt GoodCACert.crt UserNoticeQualifierTest16EE.crt
test_policychecker NIST-Test.4.8.17 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt GoodCACert.crt UserNoticeQualifierTest17EE.crt
test_policychecker NIST-Test.4.8.18.1 ENE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.1}" TrustAnchorRootCertificate.crt PoliciesP12CACert.crt UserNoticeQualifierTest18EE.crt
test_policychecker NIST-Test.4.8.18.2 ENE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.2}" TrustAnchorRootCertificate.crt PoliciesP12CACert.crt UserNoticeQualifierTest18EE.crt
test_policychecker NIST-Test.4.8.19 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt UserNoticeQualifierTest19EE.crt
test_policychecker NIST-Test.4.8.20 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt GoodCACert.crt CPSPointerQualifierTest20EE.crt
test_policychecker NIST-Test.4.9.1 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt requireExplicitPolicy10CACert.crt requireExplicitPolicy10subCACert.crt requireExplicitPolicy10subsubCACert.crt requireExplicitPolicy10subsubsubCACert.crt ValidrequireExplicitPolicyTest1EE.crt
test_policychecker NIST-Test.4.9.2 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt requireExplicitPolicy5CACert.crt requireExplicitPolicy5subCACert.crt requireExplicitPolicy5subsubCACert.crt requireExplicitPolicy5subsubsubCACert.crt ValidrequireExplicitPolicyTest2EE.crt
test_policychecker NIST-Test.4.9.3 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt requireExplicitPolicy4CACert.crt requireExplicitPolicy4subCACert.crt requireExplicitPolicy4subsubCACert.crt requireExplicitPolicy4subsubsubCACert.crt InvalidrequireExplicitPolicyTest3EE.crt
test_policychecker NIST-Test.4.9.4 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt requireExplicitPolicy0CACert.crt requireExplicitPolicy0subCACert.crt requireExplicitPolicy0subsubCACert.crt requireExplicitPolicy0subsubsubCACert.crt ValidrequireExplicitPolicyTest4EE.crt
test_policychecker NIST-Test.4.9.5 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt requireExplicitPolicy7CACert.crt requireExplicitPolicy7subCARE2Cert.crt requireExplicitPolicy7subsubCARE2RE4Cert.crt requireExplicitPolicy7subsubsubCARE2RE4Cert.crt InvalidrequireExplicitPolicyTest5EE.crt
test_policychecker NIST-Test.4.9.6 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt requireExplicitPolicy2CACert.crt requireExplicitPolicy2SelfIssuedCACert.crt ValidSelfIssuedrequireExplicitPolicyTest6EE.crt
test_policychecker NIST-Test.4.9.7 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt requireExplicitPolicy2CACert.crt requireExplicitPolicy2SelfIssuedCACert.crt requireExplicitPolicy2subCACert.crt InvalidSelfIssuedrequireExplicitPolicyTest7EE.crt
test_policychecker NIST-Test.4.9.8 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt requireExplicitPolicy2CACert.crt requireExplicitPolicy2SelfIssuedCACert.crt requireExplicitPolicy2subCACert.crt requireExplicitPolicy2SelfIssuedsubCACert.crt InvalidSelfIssuedrequireExplicitPolicyTest8EE.crt
test_policychecker NIST-Test.4.10.1.1 ENE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.1}" TrustAnchorRootCertificate.crt Mapping1to2CACert.crt ValidPolicyMappingTest1EE.crt
test_policychecker NIST-Test.4.10.1.2 EE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.2}" TrustAnchorRootCertificate.crt Mapping1to2CACert.crt ValidPolicyMappingTest1EE.crt
test_policychecker NIST-Test.4.10.1.3 EE $NIST ../../certs "{2.5.29.32.0}" P TrustAnchorRootCertificate.crt Mapping1to2CACert.crt ValidPolicyMappingTest1EE.crt
test_policychecker NIST-Test.4.10.2.1 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt Mapping1to2CACert.crt InvalidPolicyMappingTest2EE.crt
test_policychecker NIST-Test.4.10.2.2 EE $NIST ../../certs "{2.5.29.32.0}" P TrustAnchorRootCertificate.crt Mapping1to2CACert.crt InvalidPolicyMappingTest2EE.crt
test_policychecker NIST-Test.4.10.3.1 EE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.1}" TrustAnchorRootCertificate.crt P12Mapping1to3CACert.crt P12Mapping1to3subCACert.crt P12Mapping1to3subsubCACert.crt ValidPolicyMappingTest3EE.crt
test_policychecker NIST-Test.4.10.3.2 ENE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.2}" TrustAnchorRootCertificate.crt P12Mapping1to3CACert.crt P12Mapping1to3subCACert.crt P12Mapping1to3subsubCACert.crt ValidPolicyMappingTest3EE.crt
test_policychecker NIST-Test.4.10.4 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt P12Mapping1to3CACert.crt P12Mapping1to3subCACert.crt P12Mapping1to3subsubCACert.crt InvalidPolicyMappingTest4EE.crt
test_policychecker NIST-Test.4.10.5.1 ENE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.1}" TrustAnchorRootCertificate.crt P1Mapping1to234CACert.crt P1Mapping1to234subCACert.crt ValidPolicyMappingTest5EE.crt
test_policychecker NIST-Test.4.10.5.2 EE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.6}" TrustAnchorRootCertificate.crt P1Mapping1to234CACert.crt P1Mapping1to234subCACert.crt ValidPolicyMappingTest5EE.crt
test_policychecker NIST-Test.4.10.6.1 ENE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.1}" TrustAnchorRootCertificate.crt P1Mapping1to234CACert.crt P1Mapping1to234subCACert.crt ValidPolicyMappingTest6EE.crt
test_policychecker NIST-Test.4.10.6.2 EE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.6}" TrustAnchorRootCertificate.crt P1Mapping1to234CACert.crt P1Mapping1to234subCACert.crt ValidPolicyMappingTest6EE.crt TrustAnchorRootCertificate.crt
test_policychecker NIST-Test.4.10.7.1 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt MappingFromanyPolicyCACert.crt
test_policychecker NIST-Test.4.10.7.2 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt MappingFromanyPolicyCACert.crt InvalidMappingFromanyPolicyTest7EE.crt
test_policychecker NIST-Test.4.10.8.1 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt MappingToanyPolicyCACert.crt
test_policychecker NIST-Test.4.10.8.2 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt MappingToanyPolicyCACert.crt InvalidMappingToanyPolicyTest8EE.crt
test_policychecker NIST-Test.4.10.9 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt PanyPolicyMapping1to2CACert.crt ValidPolicyMappingTest9EE.crt
test_policychecker NIST-Test.4.10.10 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt GoodCACert.crt GoodsubCAPanyPolicyMapping1to2CACert.crt InvalidPolicyMappingTest10EE.crt
test_policychecker NIST-Test.4.10.11 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt GoodCACert.crt GoodsubCAPanyPolicyMapping1to2CACert.crt ValidPolicyMappingTest11EE.crt
test_policychecker NIST-Test.4.10.12.1 ENE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.1}" TrustAnchorRootCertificate.crt P12Mapping1to3CACert.crt ValidPolicyMappingTest12EE.crt
test_policychecker NIST-Test.4.10.12.2 ENE $NIST ../../certs "{2.16.840.1.101.3.2.1.48.2}" TrustAnchorRootCertificate.crt P12Mapping1to3CACert.crt ValidPolicyMappingTest12EE.crt
test_policychecker NIST-Test.4.10.13 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt P1anyPolicyMapping1to2CACert.crt ValidPolicyMappingTest13EE.crt
test_policychecker NIST-Test.4.10.14 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt P1anyPolicyMapping1to2CACert.crt ValidPolicyMappingTest14EE.crt
test_policychecker NIST-Test.4.11.1.1 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt inhibitPolicyMapping0CACert.crt inhibitPolicyMapping0subCACert.crt
test_policychecker NIST-Test.4.11.1.2 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt inhibitPolicyMapping0CACert.crt inhibitPolicyMapping0subCACert.crt InvalidinhibitPolicyMappingTest1EE.crt
test_policychecker NIST-Test.4.11.2 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt inhibitPolicyMapping1P12CACert.crt inhibitPolicyMapping1P12subCACert.crt ValidinhibitPolicyMappingTest2EE.crt
test_policychecker NIST-Test.4.11.3 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt inhibitPolicyMapping1P12CACert.crt inhibitPolicyMapping1P12subCACert.crt inhibitPolicyMapping1P12subsubCACert.crt InvalidinhibitPolicyMappingTest3EE.crt
test_policychecker NIST-Test.4.11.4 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt inhibitPolicyMapping1P12CACert.crt inhibitPolicyMapping1P12subCACert.crt inhibitPolicyMapping1P12subsubCACert.crt ValidinhibitPolicyMappingTest4EE.crt
test_policychecker NIST-Test.4.11.5 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt inhibitPolicyMapping5CACert.crt inhibitPolicyMapping5subCACert.crt inhibitPolicyMapping5subsubCACert.crt inhibitPolicyMapping5subsubsubCACert.crt InvalidinhibitPolicyMappingTest5EE.crt
test_policychecker NIST-Test.4.11.6 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt inhibitPolicyMapping1P12CACert.crt inhibitPolicyMapping1P12subCAIPM5Cert.crt inhibitPolicyMapping1P12subsubCAIPM5Cert.crt InvalidinhibitPolicyMappingTest6EE.crt
test_policychecker NIST-Test.4.11.7 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt inhibitPolicyMapping1P1CACert.crt inhibitPolicyMapping1P1SelfIssuedCACert.crt inhibitPolicyMapping1P1subCACert.crt ValidSelfIssuedinhibitPolicyMappingTest7EE.crt
test_policychecker NIST-Test.4.11.8 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt inhibitPolicyMapping1P1CACert.crt inhibitPolicyMapping1P1SelfIssuedCACert.crt inhibitPolicyMapping1P1subCACert.crt inhibitPolicyMapping1P1subsubCACert.crt InvalidSelfIssuedinhibitPolicyMappingTest8EE.crt
test_policychecker NIST-Test.4.11.9 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt inhibitPolicyMapping1P1CACert.crt inhibitPolicyMapping1P1SelfIssuedCACert.crt inhibitPolicyMapping1P1subCACert.crt inhibitPolicyMapping1P1subsubCACert.crt InvalidSelfIssuedinhibitPolicyMappingTest9EE.crt
test_policychecker NIST-Test.4.11.10 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt inhibitPolicyMapping1P1CACert.crt inhibitPolicyMapping1P1SelfIssuedCACert.crt inhibitPolicyMapping1P1subCACert.crt inhibitPolicyMapping1P1SelfIssuedsubCACert.crt InvalidSelfIssuedinhibitPolicyMappingTest10EE.crt
test_policychecker NIST-Test.4.11.11 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt inhibitPolicyMapping1P1CACert.crt inhibitPolicyMapping1P1SelfIssuedCACert.crt inhibitPolicyMapping1P1subCACert.crt inhibitPolicyMapping1P1SelfIssuedsubCACert.crt InvalidSelfIssuedinhibitPolicyMappingTest11EE.crt
test_policychecker NIST-Test.4.12.1 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt inhibitAnyPolicy0CACert.crt InvalidinhibitAnyPolicyTest1EE.crt
test_policychecker NIST-Test.4.12.2 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt inhibitAnyPolicy0CACert.crt ValidinhibitAnyPolicyTest2EE.crt
test_policychecker NIST-Test.4.12.3.1 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt inhibitAnyPolicy1CACert.crt inhibitAnyPolicy1subCA1Cert.crt inhibitAnyPolicyTest3EE.crt
test_policychecker NIST-Test.4.12.3.2 EE $NIST ../../certs "{2.5.29.32.0}" A TrustAnchorRootCertificate.crt inhibitAnyPolicy1CACert.crt inhibitAnyPolicy1subCA1Cert.crt inhibitAnyPolicyTest3EE.crt
test_policychecker NIST-Test.4.12.4 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt inhibitAnyPolicy1CACert.crt inhibitAnyPolicy1subCA1Cert.crt InvalidinhibitAnyPolicyTest4EE.crt
test_policychecker NIST-Test.4.12.5 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt inhibitAnyPolicy5CACert.crt inhibitAnyPolicy5subCACert.crt inhibitAnyPolicy5subsubCACert.crt InvalidinhibitAnyPolicyTest5EE.crt
test_policychecker NIST-Test.4.12.6 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt inhibitAnyPolicy1CACert.crt inhibitAnyPolicy1subCAIAP5Cert.crt InvalidinhibitAnyPolicyTest6EE.crt
test_policychecker NIST-Test.4.12.7 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt inhibitAnyPolicy1CACert.crt inhibitAnyPolicy1SelfIssuedCACert.crt inhibitAnyPolicy1subCA2Cert.crt ValidSelfIssuedinhibitAnyPolicyTest7EE.crt
test_policychecker NIST-Test.4.12.8 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt inhibitAnyPolicy1CACert.crt inhibitAnyPolicy1SelfIssuedCACert.crt inhibitAnyPolicy1subCA2Cert.crt inhibitAnyPolicy1subsubCA2Cert.crt InvalidSelfIssuedinhibitAnyPolicyTest8EE.crt
test_policychecker NIST-Test.4.12.9 ENE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt inhibitAnyPolicy1CACert.crt inhibitAnyPolicy1SelfIssuedCACert.crt inhibitAnyPolicy1subCA2Cert.crt inhibitAnyPolicy1SelfIssuedsubCA2Cert.crt ValidSelfIssuedinhibitAnyPolicyTest9EE.crt
test_policychecker NIST-Test.4.12.10 EE $NIST ../../certs "{2.5.29.32.0}" TrustAnchorRootCertificate.crt inhibitAnyPolicy1CACert.crt inhibitAnyPolicy1SelfIssuedCACert.crt inhibitAnyPolicy1subCA2Cert.crt InvalidSelfIssuedinhibitAnyPolicyTest10EE.crt
test_basicconstraintschecker NIST-Test.4.13.1 ENE $NIST TrustAnchorRootCertificate.crt nameConstraintsDN1CACert.crt ValidDNnameConstraintsTest1EE.crt
test_basicconstraintschecker NIST-Test.4.13.2 EE $NIST TrustAnchorRootCertificate.crt nameConstraintsDN1CACert.crt InvalidDNnameConstraintsTest2EE.crt
test_basicconstraintschecker NIST-Test.4.13.3 EE $NIST TrustAnchorRootCertificate.crt nameConstraintsDN1CACert.crt InvalidDNnameConstraintsTest3EE.crt
test_basicconstraintschecker NIST-Test.4.13.4 ENE $NIST TrustAnchorRootCertificate.crt nameConstraintsDN1CACert.crt ValidDNnameConstraintsTest4EE.crt
test_basicconstraintschecker NIST-Test.4.13.5 ENE $NIST TrustAnchorRootCertificate.crt nameConstraintsDN2CACert.crt ValidDNnameConstraintsTest5EE.crt
test_basicconstraintschecker NIST-Test.4.13.6 ENE $NIST TrustAnchorRootCertificate.crt nameConstraintsDN3CACert.crt ValidDNnameConstraintsTest6EE.crt
test_basicconstraintschecker NIST-Test.4.13.7 EE $NIST TrustAnchorRootCertificate.crt nameConstraintsDN3CACert.crt InvalidDNnameConstraintsTest7EE.crt
test_basicconstraintschecker NIST-Test.4.13.8 EE $NIST TrustAnchorRootCertificate.crt nameConstraintsDN4CACert.crt InvalidDNnameConstraintsTest8EE.crt
test_basicconstraintschecker NIST-Test.4.13.9 EE $NIST TrustAnchorRootCertificate.crt nameConstraintsDN4CACert.crt InvalidDNnameConstraintsTest9EE.crt
test_basicconstraintschecker NIST-Test.4.13.10 EE $NIST TrustAnchorRootCertificate.crt nameConstraintsDN5CACert.crt InvalidDNnameConstraintsTest10EE.crt
test_basicconstraintschecker NIST-Test.4.13.11 ENE $NIST TrustAnchorRootCertificate.crt nameConstraintsDN5CACert.crt ValidDNnameConstraintsTest11EE.crt
test_basicconstraintschecker NIST-Test.4.13.12 EE $NIST TrustAnchorRootCertificate.crt nameConstraintsDN1CACert.crt nameConstraintsDN1subCA1Cert.crt InvalidDNnameConstraintsTest12EE.crt
test_basicconstraintschecker NIST-Test.4.13.13 EE $NIST TrustAnchorRootCertificate.crt nameConstraintsDN1CACert.crt nameConstraintsDN1subCA2Cert.crt InvalidDNnameConstraintsTest13EE.crt
test_basicconstraintschecker NIST-Test.4.13.14 ENE $NIST TrustAnchorRootCertificate.crt nameConstraintsDN1CACert.crt nameConstraintsDN1subCA2Cert.crt ValidDNnameConstraintsTest14EE.crt
test_basicconstraintschecker NIST-Test.4.13.15 EE $NIST TrustAnchorRootCertificate.crt nameConstraintsDN3CACert.crt nameConstraintsDN3subCA1Cert.crt InvalidDNnameConstraintsTest15EE.crt
test_basicconstraintschecker NIST-Test.4.13.16 EE $NIST TrustAnchorRootCertificate.crt nameConstraintsDN3CACert.crt nameConstraintsDN3subCA1Cert.crt InvalidDNnameConstraintsTest16EE.crt
test_basicconstraintschecker NIST-Test.4.13.17 EE $NIST TrustAnchorRootCertificate.crt nameConstraintsDN3CACert.crt nameConstraintsDN3subCA2Cert.crt InvalidDNnameConstraintsTest17EE.crt
test_basicconstraintschecker NIST-Test.4.13.18 ENE $NIST TrustAnchorRootCertificate.crt nameConstraintsDN3CACert.crt nameConstraintsDN3subCA2Cert.crt ValidDNnameConstraintsTest18EE.crt
test_basicconstraintschecker NIST-Test.4.13.19 ENE $NIST TrustAnchorRootCertificate.crt nameConstraintsDN1CACert.crt nameConstraintsDN1SelfIssuedCACert.crt ValidDNnameConstraintsTest19EE.crt
test_basicconstraintschecker NIST-Test.4.13.20 EE $NIST TrustAnchorRootCertificate.crt nameConstraintsDN1CACert.crt InvalidDNnameConstraintsTest20EE.crt
test_basicconstraintschecker NIST-Test.4.13.21 ENE $NIST TrustAnchorRootCertificate.crt nameConstraintsRFC822CA1Cert.crt ValidRFC822nameConstraintsTest21EE.crt
test_basicconstraintschecker NIST-Test.4.13.22 EE $NIST TrustAnchorRootCertificate.crt nameConstraintsRFC822CA1Cert.crt InvalidRFC822nameConstraintsTest22EE.crt
test_basicconstraintschecker NIST-Test.4.13.23 ENE $NIST TrustAnchorRootCertificate.crt nameConstraintsRFC822CA2Cert.crt ValidRFC822nameConstraintsTest23EE.crt
test_basicconstraintschecker NIST-Test.4.13.24 EE $NIST TrustAnchorRootCertificate.crt nameConstraintsRFC822CA2Cert.crt InvalidRFC822nameConstraintsTest24EE.crt
test_basicconstraintschecker NIST-Test.4.13.25 ENE $NIST TrustAnchorRootCertificate.crt nameConstraintsRFC822CA3Cert.crt ValidRFC822nameConstraintsTest25EE.crt
test_basicconstraintschecker NIST-Test.4.13.26 EE $NIST TrustAnchorRootCertificate.crt nameConstraintsRFC822CA3Cert.crt InvalidRFC822nameConstraintsTest26EE.crt
test_basicconstraintschecker NIST-Test.4.13.27 ENE $NIST TrustAnchorRootCertificate.crt nameConstraintsDN1CACert.crt  nameConstraintsDN1subCA3Cert.crt ValidDNandRFC822nameConstraintsTest27EE.crt
test_basicconstraintschecker NIST-Test.4.13.28 EE $NIST TrustAnchorRootCertificate.crt nameConstraintsDN1CACert.crt  nameConstraintsDN1subCA3Cert.crt InvalidDNandRFC822nameConstraintsTest28EE.crt
test_basicconstraintschecker NIST-Test.4.13.29 EE $NIST TrustAnchorRootCertificate.crt nameConstraintsDN1CACert.crt  nameConstraintsDN1subCA3Cert.crt InvalidDNandRFC822nameConstraintsTest29EE.crt
test_basicconstraintschecker NIST-Test.4.13.30 ENE $NIST TrustAnchorRootCertificate.crt nameConstraintsDNS1CACert.crt ValidDNSnameConstraintsTest30EE.crt
test_basicconstraintschecker NIST-Test.4.13.31 EE $NIST TrustAnchorRootCertificate.crt nameConstraintsDNS1CACert.crt InvalidDNSnameConstraintsTest31EE.crt
test_basicconstraintschecker NIST-Test.4.13.32 ENE $NIST TrustAnchorRootCertificate.crt nameConstraintsDNS2CACert.crt ValidDNSnameConstraintsTest32EE.crt
test_basicconstraintschecker NIST-Test.4.13.33 EE $NIST TrustAnchorRootCertificate.crt nameConstraintsDNS2CACert.crt InvalidDNSnameConstraintsTest33EE.crt
test_basicconstraintschecker NIST-Test.4.13.34 ENE $NIST TrustAnchorRootCertificate.crt nameConstraintsURI1CACert.crt ValidURInameConstraintsTest34EE.crt
test_basicconstraintschecker NIST-Test.4.13.35 EE $NIST TrustAnchorRootCertificate.crt nameConstraintsURI1CACert.crt InvalidURInameConstraintsTest35EE.crt
test_basicconstraintschecker NIST-Test.4.13.36 ENE $NIST TrustAnchorRootCertificate.crt nameConstraintsURI2CACert.crt ValidURInameConstraintsTest36EE.crt
test_basicconstraintschecker NIST-Test.4.13.37 EE $NIST TrustAnchorRootCertificate.crt nameConstraintsURI2CACert.crt InvalidURInameConstraintsTest37EE.crt
test_basicconstraintschecker NIST-Test.4.13.38 EE $NIST TrustAnchorRootCertificate.crt nameConstraintsDNS1CACert.crt InvalidDNSnameConstraintsTest38EE.crt
test_basicconstraintschecker NIST-Test.4.16.1 ENE $NIST TrustAnchorRootCertificate.crt ValidUnknownNotCriticalCertificateExtensionTest1EE.crt
test_basicconstraintschecker NIST-Test.4.16.2 EE $NIST TrustAnchorRootCertificate.crt InvalidUnknownCriticalCertificateExtensionTest2EE.crt
test_buildchain_uchecker NIST-Test.4.1.1-without-OID ENE - $NIST ValidCertificatePathTest1EE.crt GoodCACert.crt TrustAnchorRootCertificate.crt
test_buildchain_uchecker NIST-Test.4.1.1-with-OID-without-forwardSupport ENE 2.5.29.19 $NIST ValidCertificatePathTest1EE.crt GoodCACert.crt TrustAnchorRootCertificate.crt
test_buildchain_uchecker NIST-Test.4.1.1-with-OID-forwardSupport ENE F2.5.29.19 $NIST ValidCertificatePathTest1EE.crt GoodCACert.crt TrustAnchorRootCertificate.crt
test_buildchain ${LDAP}  NIST-Test.4.1.1 ENE $NIST ValidCertificatePathTest1EE.crt GoodCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.1.2 EE $NIST InvalidCASignatureTest2EE.crt BadSignedCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.1.3 EE $NIST InvalidEESignatureTest3EE.crt GoodCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.1.4 ENE $NIST ValidDSASignaturesTest4EE.crt DSACACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.1.5 ENE $NIST ValidDSAParameterInheritanceTest5EE.crt DSAParametersInheritedCACert.crt DSACACert.crt TrustAnchorRootCertificate.crt  
test_buildchain ${LDAP}  NIST-Test.4.1.6 EE $NIST InvalidDSASignatureTest6EE.crt DSACACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.2.1 EE $NIST InvalidCAnotBeforeDateTest1EE.crt BadnotBeforeDateCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.2.2 EE $NIST InvalidEEnotBeforeDateTest2EE.crt GoodCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.2.3 ENE $NIST Validpre2000UTCnotBeforeDateTest3EE.crt GoodCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.2.4 ENE $NIST ValidGeneralizedTimenotBeforeDateTest4EE.crt GoodCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.2.5 EE $NIST InvalidCAnotAfterDateTest5EE.crt BadnotAfterDateCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.2.6 EE $NIST InvalidEEnotAfterDateTest6EE.crt GoodCACert.crt TrustAnchorRootCertificate.crt
test_buildchain ${LDAP}  NIST-Test.4.2.7 EE $NIST Invalidpre2000UTCEEnotAfterDateTest7EE.crt GoodCACert.crt TrustAnchorRootCertificate.crt
test_buildchain ${LDAP}  NIST-Test.4.2.8 ENE $NIST ValidGeneralizedTimenotAfterDateTest8EE.crt GoodCACert.crt TrustAnchorRootCertificate.crt
test_buildchain ${LDAP}  NIST-Test.4.3.1 EE $NIST InvalidNameChainingTest1EE.crt GoodCACert.crt TrustAnchorRootCertificate.crt
test_buildchain ${LDAP}  NIST-Test.4.3.2 EE $NIST InvalidNameChainingOrderTest2EE.crt NameOrderingCACert.crt TrustAnchorRootCertificate.crt
test_buildchain ${LDAP}  NIST-Test.4.3.3 ENE $NIST ValidNameChainingWhitespaceTest3EE.crt GoodCACert.crt TrustAnchorRootCertificate.crt
test_buildchain ${LDAP}  NIST-Test.4.3.4 ENE $NIST ValidNameChainingWhitespaceTest4EE.crt GoodCACert.crt TrustAnchorRootCertificate.crt
test_buildchain ${LDAP}  NIST-Test.4.3.5 ENE $NIST ValidNameChainingCapitalizationTest5EE.crt GoodCACert.crt TrustAnchorRootCertificate.crt
test_buildchain ${LDAP}  NIST-Test.4.3.6 ENE $NIST ValidNameUIDsTest6EE.crt UIDCACert.crt TrustAnchorRootCertificate.crt
test_buildchain -  NIST-Test.4.3.7 ENE $NIST ValidRFC3280MandatoryAttributeTypesTest7EE.crt RFC3280MandatoryAttributeTypesCACert.crt TrustAnchorRootCertificate.crt
test_buildchain ${LDAP}  NIST-Test.4.3.9 ENE $NIST ValidUTF8StringEncodedNamesTest9EE.crt UTF8StringEncodedNamesCACert.crt TrustAnchorRootCertificate.crt
test_buildchain ${LDAP}  NIST-Test.4.3.10 ENE $NIST ValidRolloverfromPrintableStringtoUTF8StringTest10EE.crt RolloverfromPrintableStringtoUTF8StringCACert.crt TrustAnchorRootCertificate.crt
test_buildchain ${LDAP}  NIST-Test.4.3.11 ENE $NIST ValidUTF8StringCaseInsensitiveMatchTest11EE.crt UTF8StringCaseInsensitiveMatchCACert.crt TrustAnchorRootCertificate.crt
test_buildchain ${LDAP}  NIST-Test.4.4.1 EE $NIST InvalidMissingCRLTest1EE.crt NoCRLCACert.crt TrustAnchorRootCertificate.crt
test_buildchain ${LDAP}  NIST-Test.4.4.2 EE $NIST InvalidRevokedCATest2EE.crt RevokedsubCACert.crt GoodCACert.crt TrustAnchorRootCertificate.crt
test_buildchain ${LDAP}  NIST-Test.4.4.3 EE $NIST InvalidRevokedEETest3EE.crt GoodCACert.crt TrustAnchorRootCertificate.crt
test_buildchain ${LDAP}  NIST-Test.4.4.4 EE $NIST InvalidBadCRLSignatureTest4EE.crt BadCRLSignatureCACert.crt TrustAnchorRootCertificate.crt
test_buildchain ${LDAP}  NIST-Test.4.4.5 EE $NIST InvalidBadCRLIssuerNameTest5EE.crt BadCRLIssuerNameCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.4.6 EE $NIST InvalidWrongCRLTest6EE.crt WrongCRLCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.4.7 ENE $NIST ValidTwoCRLsTest7EE.crt TwoCRLsCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.4.8 EE $NIST InvalidUnknownCRLEntryExtensionTest8EE.crt UnknownCRLEntryExtensionCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.4.9 EE $NIST InvalidUnknownCRLExtensionTest9EE.crt UnknownCRLExtensionCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.4.10 EE $NIST InvalidUnknownCRLExtensionTest10EE.crt UnknownCRLExtensionCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.4.11 EE $NIST InvalidOldCRLnextUpdateTest11EE.crt OldCRLnextUpdateCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.4.12 EE $NIST Invalidpre2000CRLnextUpdateTest12EE.crt pre2000CRLnextUpdateCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.4.13 ENE $NIST ValidGeneralizedTimeCRLnextUpdateTest13EE.crt GeneralizedTimeCRLnextUpdateCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.4.14 ENE $NIST ValidNegativeSerialNumberTest14EE.crt NegativeSerialNumberCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.4.15 EE $NIST InvalidNegativeSerialNumberTest15EE.crt NegativeSerialNumberCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.4.16 ENE $NIST ValidLongSerialNumberTest16EE.crt LongSerialNumberCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.4.17 ENE $NIST ValidLongSerialNumberTest17EE.crt LongSerialNumberCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.4.18 EE $NIST InvalidLongSerialNumberTest18EE.crt LongSerialNumberCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.4.20 EE $NIST InvalidSeparateCertificateandCRLKeysTest20EE.crt SeparateCertificateandCRLKeysCRLSigningCert.crt TrustAnchorRootCertificate.crt SeparateCertificateandCRLKeysCertificateSigningCACert.crt 
test_buildchain ${LDAP}  NIST-Test.4.5.1 ENE $NIST ValidBasicSelfIssuedOldWithNewTest1EE.crt BasicSelfIssuedNewKeyOldWithNewCACert.crt BasicSelfIssuedNewKeyCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.5.2 EE $NIST InvalidBasicSelfIssuedOldWithNewTest2EE.crt BasicSelfIssuedNewKeyOldWithNewCACert.crt BasicSelfIssuedNewKeyCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.6.1 EE $NIST InvalidMissingbasicConstraintsTest1EE.crt MissingbasicConstraintsCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.6.2 EE $NIST InvalidcAFalseTest2EE.crt basicConstraintsCriticalcAFalseCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.6.3 EE $NIST InvalidcAFalseTest3EE.crt basicConstraintsNotCriticalcAFalseCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.6.4 ENE $NIST ValidbasicConstraintsNotCriticalTest4EE.crt basicConstraintsNotCriticalCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.6.5 EE $NIST InvalidpathLenConstraintTest5EE.crt pathLenConstraint0subCACert.crt pathLenConstraint0CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.6.6 EE $NIST InvalidpathLenConstraintTest6EE.crt pathLenConstraint0subCACert.crt pathLenConstraint0CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.6.7 ENE $NIST ValidpathLenConstraintTest7EE.crt pathLenConstraint0CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.6.8 ENE $NIST ValidpathLenConstraintTest8EE.crt pathLenConstraint0CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.6.9 EE $NIST InvalidpathLenConstraintTest9EE.crt pathLenConstraint6subCA0Cert.crt pathLenConstraint6subsubCA00Cert.crt pathLenConstraint6CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.6.10 EE $NIST InvalidpathLenConstraintTest10EE.crt pathLenConstraint6subsubCA00Cert.crt pathLenConstraint6subCA0Cert.crt pathLenConstraint6CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.6.11 EE $NIST InvalidpathLenConstraintTest11EE.crt pathLenConstraint6subsubsubCA11XCert.crt pathLenConstraint6subsubCA11Cert.crt pathLenConstraint6subCA1Cert.crt pathLenConstraint6CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.6.12 EE $NIST InvalidpathLenConstraintTest12EE.crt pathLenConstraint6subsubsubCA11XCert.crt pathLenConstraint6subsubCA11Cert.crt pathLenConstraint6subCA1Cert.crt pathLenConstraint6CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.6.13 ENE $NIST ValidpathLenConstraintTest13EE.crt pathLenConstraint6subsubsubCA41XCert.crt pathLenConstraint6subsubCA41Cert.crt pathLenConstraint6subCA4Cert.crt pathLenConstraint6CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.6.14 ENE $NIST ValidpathLenConstraintTest14EE.crt pathLenConstraint6subsubsubCA41XCert.crt pathLenConstraint6subsubCA41Cert.crt pathLenConstraint6subCA4Cert.crt pathLenConstraint6CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.6.15 ENE $NIST ValidSelfIssuedpathLenConstraintTest15EE.crt pathLenConstraint0SelfIssuedCACert.crt pathLenConstraint0CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.6.16 EE $NIST InvalidSelfIssuedpathLenConstraintTest16EE.crt pathLenConstraint0subCA2Cert.crt pathLenConstraint0SelfIssuedCACert.crt pathLenConstraint0CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.6.17 ENE $NIST ValidSelfIssuedpathLenConstraintTest17EE.crt pathLenConstraint1SelfIssuedsubCACert.crt pathLenConstraint1subCACert.crt pathLenConstraint1SelfIssuedCACert.crt pathLenConstraint1CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.7.1 EE $NIST InvalidkeyUsageCriticalkeyCertSignFalseTest1EE.crt keyUsageCriticalkeyCertSignFalseCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.7.2 EE $NIST InvalidkeyUsageNotCriticalkeyCertSignFalseTest2EE.crt keyUsageNotCriticalkeyCertSignFalseCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.7.3 ENE $NIST ValidkeyUsageNotCriticalTest3EE.crt keyUsageNotCriticalCACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.7.4 EE $NIST InvalidkeyUsageCriticalcRLSignFalseTest4EE.crt keyUsageCriticalcRLSignFalseCACert.crt TrustAnchorRootCertificate.crt  
test_buildchain ${LDAP}  NIST-Test.4.7.5 EE $NIST InvalidkeyUsageNotCriticalcRLSignFalseTest5EE.crt keyUsageNotCriticalcRLSignFalseCACert.crt TrustAnchorRootCertificate.crt  
test_buildchain ${LDAP}  NIST-Test.4.13.1 ENE $NIST ValidDNnameConstraintsTest1EE.crt nameConstraintsDN1CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.2 EE $NIST InvalidDNnameConstraintsTest2EE.crt nameConstraintsDN1CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.3 EE $NIST InvalidDNnameConstraintsTest3EE.crt nameConstraintsDN1CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.4 ENE $NIST ValidDNnameConstraintsTest4EE.crt nameConstraintsDN1CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.5 ENE $NIST ValidDNnameConstraintsTest5EE.crt nameConstraintsDN2CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.6 ENE $NIST ValidDNnameConstraintsTest6EE.crt nameConstraintsDN3CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.7 EE $NIST InvalidDNnameConstraintsTest7EE.crt nameConstraintsDN3CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.8 EE $NIST InvalidDNnameConstraintsTest8EE.crt nameConstraintsDN4CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.9 EE $NIST InvalidDNnameConstraintsTest9EE.crt nameConstraintsDN4CACert.crt nameConstraintsDN4CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.10 EE $NIST InvalidDNnameConstraintsTest10EE.crt nameConstraintsDN5CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.11 ENE $NIST ValidDNnameConstraintsTest11EE.crt nameConstraintsDN5CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.12 EE $NIST InvalidDNnameConstraintsTest12EE.crt nameConstraintsDN1subCA1Cert.crt nameConstraintsDN1CACert.crt TrustAnchorRootCertificate.crt
test_buildchain ${LDAP}  NIST-Test.4.13.13 EE $NIST InvalidDNnameConstraintsTest13EE.crt nameConstraintsDN1subCA2Cert.crt nameConstraintsDN1subCA2Cert.crt nameConstraintsDN1CACert.crt TrustAnchorRootCertificate.crt
test_buildchain ${LDAP}  NIST-Test.4.13.14 ENE $NIST ValidDNnameConstraintsTest14EE.crt  nameConstraintsDN1subCA2Cert.crt nameConstraintsDN1CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.15 EE $NIST InvalidDNnameConstraintsTest15EE.crt nameConstraintsDN3subCA1Cert.crt nameConstraintsDN3CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.16 EE $NIST InvalidDNnameConstraintsTest16EE.crt nameConstraintsDN3subCA1Cert.crt nameConstraintsDN3CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.17 EE $NIST InvalidDNnameConstraintsTest17EE.crt nameConstraintsDN3subCA2Cert.crt nameConstraintsDN3CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.18 ENE $NIST ValidDNnameConstraintsTest18EE.crt nameConstraintsDN3subCA2Cert.crt nameConstraintsDN3CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.19 ENE $NIST ValidDNnameConstraintsTest19EE.crt nameConstraintsDN1SelfIssuedCACert.crt nameConstraintsDN1CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.20 EE $NIST InvalidDNnameConstraintsTest20EE.crt nameConstraintsDN1CACert.crt TrustAnchorRootCertificate.crt
test_buildchain ${LDAP}  NIST-Test.4.13.21 ENE $NIST ValidRFC822nameConstraintsTest21EE.crt nameConstraintsRFC822CA1Cert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.22 EE $NIST InvalidRFC822nameConstraintsTest22EE.crt nameConstraintsRFC822CA1Cert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.23 ENE $NIST ValidRFC822nameConstraintsTest23EE.crt nameConstraintsRFC822CA2Cert.crt  TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.24 EE $NIST InvalidRFC822nameConstraintsTest24EE.crt nameConstraintsRFC822CA2Cert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.25 ENE $NIST ValidRFC822nameConstraintsTest25EE.crt nameConstraintsRFC822CA3Cert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.26 EE $NIST InvalidRFC822nameConstraintsTest26EE.crt nameConstraintsRFC822CA3Cert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.27 ENE $NIST ValidDNandRFC822nameConstraintsTest27EE.crt nameConstraintsDN1subCA3Cert.crt nameConstraintsDN1CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.28 EE $NIST InvalidDNandRFC822nameConstraintsTest28EE.crt nameConstraintsDN1subCA3Cert.crt nameConstraintsDN1CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.29 EE $NIST InvalidDNandRFC822nameConstraintsTest29EE.crt nameConstraintsDN1subCA3Cert.crt nameConstraintsDN1CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.30 ENE $NIST ValidDNSnameConstraintsTest30EE.crt nameConstraintsDNS1CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.31 EE $NIST InvalidDNSnameConstraintsTest31EE.crt nameConstraintsDNS1CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.32 ENE $NIST ValidDNSnameConstraintsTest32EE.crt nameConstraintsDNS2CACert.crt TrustAnchorRootCertificate.crt
test_buildchain ${LDAP}  NIST-Test.4.13.33 EE $NIST InvalidDNSnameConstraintsTest33EE.crt nameConstraintsDNS2CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.34 ENE $NIST ValidURInameConstraintsTest34EE.crt nameConstraintsURI1CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.35 EE $NIST InvalidURInameConstraintsTest35EE.crt nameConstraintsURI1CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.36 ENE $NIST ValidURInameConstraintsTest36EE.crt nameConstraintsURI2CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.37 EE $NIST InvalidURInameConstraintsTest37EE.crt nameConstraintsURI2CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP}  NIST-Test.4.13.38 EE $NIST InvalidDNSnameConstraintsTest38EE.crt nameConstraintsDNS1CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain_partialchain ${LDAP}  NIST-Test.4.6.14 ENE $NIST ValidpathLenConstraintTest14EE.crt pathLenConstraint6subsubsubCA41XCert.crt pathLenConstraint6subsubCA41Cert.crt pathLenConstraint6subCA4Cert.crt pathLenConstraint6CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain_partialchain ${LDAP}  NIST-Test.4.6.14 ENE $NIST ValidpathLenConstraintTest14EE.crt pathLenConstraint6subsubsubCA41XCert.crt pathLenConstraint6subsubCA41Cert.crt TrustAnchorRootCertificate.crt pathLenConstraint6subCA4Cert.crt pathLenConstraint6CACert.crt TrustAnchorRootCertificate.crt 
test_buildchain_partialchain ${LDAP}  NIST-Test.4.13.13 EE $NIST InvalidDNnameConstraintsTest13EE.crt nameConstraintsDN1subCA2Cert.crt nameConstraintsDN1subCA2Cert.crt nameConstraintsDN1CACert.crt TrustAnchorRootCertificate.crt
test_buildchain_partialchain ${LDAP}  NIST-Test.4.13.27 ENE $NIST ValidDNandRFC822nameConstraintsTest27EE.crt nameConstraintsDN1subCA3Cert.crt nameConstraintsDN1subCA2Cert.crt TrustAnchorRootCertificate.crt 
test_buildchain ${LDAP} NIST-PDTest ENE ${NIST_PDTEST} certs/BasicHTTPURIPathDiscoveryTest2EE.crt certs/BasicHTTPURITrustAnchorRootCert.crt
test_ocsp OCSP-Test ENE ${HOSTDIR}/ocsp anchorcert.crt goodcert.crt
test_ocsp OCSP-Test EE ${HOSTDIR}/ocsp anchorcert.crt revokedcert.crt
EOF

totalErrors=$?
totalErrors=`expr ${totalErrors} + ${tracedErrors}`

html_msg ${totalErrors} 0 "&nbsp;&nbsp;&nbsp;${testunit}: passed ${passed} of ${numtests} tests"
exit ${totalErrors}

##########################################################
#
# Document NIST tests that are not currently running for builder...
# 4.3.8  4.4.19  4.4.21
#
# Others
# 4.5.4  4.5.5, 4.5.6, 4.5.7, 4.5.8
# 4.14.* Distribution Point - functionality not yet implemented
# 4.15.* Delta CRL - not supported
##########################################################
# Following tests are not run because of bugs beyond libpkix:
#test_validatechain NIST-Test.4.3.7 ENE $NIST TrustAnchorRootCertificate.crt RFC3280MandatoryAttributeTypesCACert.crt ValidRFC3280MandatoryAttributeTypesTest7EE.crt
# test_buildchain NIST-Test.4.3.8 ENE $NIST ValidRFC3280OptionalAttributeTypesTest8EE.crt RFC3280OptionalAttributeTypesCACert.crt TrustAnchorRootCertificate.crt

# Following tests are not supported by libpkix : separate certificate
# NIST test 4.4.19 and 4.4.21

# Following tests are not supported by libpkix : cert dp, cert chain definition
# NIST tests 4.5.4, 4.5.5
#test_buildchain NIST-Test.4.5.7 EE $NIST InvalidBasicSelfIssuedCRLSigningKeyTest7EE.crt BasicSelfIssuedCRLSigningKeyCRLCert.crt TrustAnchorRootCertificate.crt BasicSelfIssuedCRLSigningKeyCACert.crt 
#test_buildchain NIST-Test.4.5.8 EE $NIST InvalidBasicSelfIssuedCRLSigningKeyTest8EE.crt BasicSelfIssuedCRLSigningKeyCRLCert.crt BasicSelfIssuedCRLSigningKeyCACert.crt TrustAnchorRootCertificate.crt 


# Following tests are not supported by libpkix : self-issued, multiple keys, one for cert, one for CRL
#test_validatechain NIST-Test.4.5.3 ENE $NIST TrustAnchorRootCertificate.crt BasicSelfIssuedOldKeyCACert.crt BasicSelfIssuedOldKeyNewWithOldCACert.crt ValidBasicSelfIssuedNewWithOldTest3EE.crt
#test_defaultcrlchecker NIST-Test.4.5.4 ENE $NIST/../crls $NIST/TrustAnchorRootCertificate.crt $NIST/BasicSelfIssuedOldKeyCACert.crt $NIST/BasicSelfIssuedOldKeyNewWithOldCACert.crt $NIST/ValidBasicSelfIssuedNewWithOldTest4EE.crt
#test_defaultcrlchecker NIST-Test.4.5.6 ENE $NIST/../crls $NIST/TrustAnchorRootCertificate.crt $NIST/BasicSelfIssuedCRLSigningKeyCACert.crt $NIST/BasicSelfIssuedCRLSigningKeyCRLCert.crt $NIST/ValidBasicSelfIssuedCRLSigningKeyTest6EE.crt

# Need to recreate certs with BC extension and Key Usage
#test_buildchain single_sig ENE build_data/single_path/signature/pass yassir2hanfei.crt greg2yassir.crt jes2greg.crt jes2jes.crt
#test_buildchain single-sig EE build_data/single_path/signature/fail yassir2hanfei.crt jes2jes.crt
#test_buildchain multi-sig ENE build_data/multi_path/signature/pass yassir2hanfei.crt greg2yassir.crt jes2greg.crt jes2jes.crt
#test_buildchain multi-sig EE build_data/multi_path/signature/fail yassir2hanfei.crt greg2yassir.crt yassir2hanfei.crt
#test_buildchain backtrack-sig ENE build_data/backtracking/signature yassir2hanfei.crt labs2yassir.crt jes2labs.crt jes2jes.crtn
