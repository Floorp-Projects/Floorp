#!/bin/sh
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
#
########################################################################
#
# mozilla/security/nss/tests/pkits/pkits.sh
#
# Script to test the NIST PKITS tests 
#
# needs to work on all Unix and Windows platforms
#
# tests implemented:
#    vfychain 
#
# special NOTES
# ---------------
# NIST PKITS data needs to be downloaded from
# http://csrc.nist.gov/pki/testing/x509paths.html
# Environment variable PKITS_DATA needs to be set to the directory
# where this data is downloaded, or test data needs to be copied under 
# the mozilla source tree in mozilla/PKITS_DATA
########################################################################



############################## pkits_init ##############################
# local shell function to initialize this script 
########################################################################
pkits_init()
{
  SCRIPTNAME=pkits.sh

  if [ -z "${CLEANUP}" ] ; then
      CLEANUP="${SCRIPTNAME}"
  fi

  if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ]; then
      cd ../common
      . ./init.sh
  fi

  html_head "NIST PKITS Tests"

  PKITSDIR=${HOSTDIR}/pkits

  COPYDIR=${PKITSDIR}/copydir

  mkdir -p ${PKITSDIR}
  mkdir -p ${COPYDIR}
  mkdir -p ${PKITSDIR}/html

  if [ ! -d "${PKITS_DATA}" ]; then
      PKITS_DATA=../../../../PKITS_DATA
  fi

  export PKITS_DATA

  certs=${PKITS_DATA}/certs
  crls=${PKITS_DATA}/crls

  cd ${PKITSDIR}

  PKITSdb=${PKITSDIR}/PKITSdb

  PKITS_LOG=${PKITSDIR}/pkits.log #getting its own logfile

  if [ ! -d "${PKITSdb}" ]; then
      mkdir -p ${PKITSdb}
  else
      echo "$SCRIPTNAME: WARNING - ${PKITSdb} exists"
  fi

  echo "HOSTDIR" $HOSTDIR
  echo "PKITSDIR" $PKITSDIR
  echo "PKITSdb" $PKITSdb
  echo "PKITS_DATA" $PKITS_DATA
  echo "certs" $certs
  echo "crls" $crls

  echo nss > ${PKITSdb}/pw
  certutil -N -d ${PKITSdb} -f ${PKITSdb}/pw
  certutil -A -n TrustAnchorRootCertificate -t "C,C,C" -i \
      $certs/TrustAnchorRootCertificate.crt -d $PKITSdb
  certutil -A -n GoodCACert -t ",," -i $certs/GoodCACert.crt -d $PKITSdb

}

############################### pkits_log ##############################
# write to pkits.log file
########################################################################
pkits_log()
{
  echo "$SCRIPTNAME $*"
  echo $* >>${PKITS_LOG}
}


################################ pkits #################################
# local shell function for positive testcases, calls vfychain, writes 
# action and options to stdout, sets variable RET and writes results to 
# the html file results
########################################################################
pkits()
{
  echo "$SCRIPTNAME: ${VFY_ACTION} --------------------------"
  echo "vfychain -d PKITSdb -u 4 $*"
  vfychain -d $PKITSdb -u 4 $* >  ${PKITSDIR}/cmdout.txt 2>&1
  RET=$(grep -c ERROR ${PKITSDIR}/cmdout.txt)
  cat ${PKITSDIR}/cmdout.txt

  if [ "$RET" -ne 0 ]; then
      html_failed "<TR><TD>${VFY_ACTION} ($RET) "
      pkits_log "ERROR: ${VFY_ACTION} failed $RET"
  else
      html_passed "<TR><TD>${VFY_ACTION}"
      pkits_log "SUCCESS: ${VFY_ACTION} returned as expected $RET"
  fi

  return $RET
}

################################ pkitsn #################################
# local shell function for negative testcases, calls vfychain, writes 
# action and options to stdout, sets variable RET and writes results to 
# the html file results
########################################################################
pkitsn()
{
  echo "$SCRIPTNAME: ${VFY_ACTION} --------------------------"
  echo "vfychain -d PKITSdb -u 4 $*"
  vfychain -d $PKITSdb -u 4 $* >  ${PKITSDIR}/cmdout.txt 2>&1
  RET=$(grep -c ERROR ${PKITSDIR}/cmdout.txt)
  cat ${PKITSDIR}/cmdout.txt

  if [ "$RET" -eq 0 ]; then
      html_failed "<TR><TD>${VFY_ACTION} ($RET) "
      pkits_log "ERROR: ${VFY_ACTION} failed $RET"
  else
      html_passed "<TR><TD>${VFY_ACTION} ($RET) "
      pkits_log "SUCCESS: ${VFY_ACTION} returned as expected $RET"
  fi
  return $RET
}

############################## pkits_tests_bySection ###################
# running the various PKITS tests
########################################################################
pkits_SignatureVerification()
{
  echo "***************************************************************"
  echo "Section 4.1: Signature Verification"
  echo "***************************************************************"

  VFY_ACTION="Valid Signatures Test1"
  pkits $certs/ValidCertificatePathTest1EE.crt $certs/GoodCACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid CA Signature Test2"
  pkitsn $certs/InvalidCASignatureTest2EE.crt \
      $certs/BadSignedCACert.crt $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid EE Signature Test3"
  pkitsn $certs/InvalidEESignatureTest3EE.crt $certs/GoodCACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid DSA Signatures Test4"
  pkits $certs/ValidDSASignaturesTest4EE.crt $certs/DSACACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid DSA Parameter Inheritance Test5"
  pkits $certs/ValidDSAParameterInheritanceTest5EE.crt \
      $certs/DSAParametersInheritedCACert.crt \
      $certs/DSACACert.crt $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid DSA Signature Test6"
  pkitsn $certs/InvalidDSASignatureTest6EE.crt $certs/DSACACert.crt \
      $certs/TrustAnchorRootCertificate.crt
}

pkits_ValidityPeriods()
{
  echo "***************************************************************"
  echo "Section 4.2: Validity Periods"
  echo "***************************************************************"

  VFY_ACTION="Invalid CA notBefore Date Test1"
  pkitsn $certs/InvalidCAnotBeforeDateTest1EE.crt \
      $certs/BadnotBeforeDateCACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid EE notBefore Date Test2"
  pkitsn $certs/InvalidEEnotBeforeDateTest2EE.crt \
      $certs/GoodCACert.crt $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid pre2000 UTC notBefore Date Test3"
  pkits $certs/Validpre2000UTCnotBeforeDateTest3EE.crt \
      $certs/GoodCACert.crt $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid GeneralizedTime notBefore Date Test4"
  pkits $certs/ValidGeneralizedTimenotBeforeDateTest4EE.crt \
      $certs/GoodCACert.crt $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid CA notAfter Date Test5"
  pkitsn $certs/InvalidCAnotAfterDateTest5EE.crt \
      $certs/BadnotAfterDateCACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid EE notAfter Date Test6"
  pkitsn $certs/InvalidEEnotAfterDateTest6EE.crt \
      $certs/GoodCACert.crt $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid pre2000 UTC EE notAfter Date Test7"
  pkitsn $certs/Invalidpre2000UTCEEnotAfterDateTest7EE.crt \
      $certs/GoodCACert.crt $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="ValidGeneralizedTime notAfter Date Test8"
  pkits $certs/ValidGeneralizedTimenotAfterDateTest8EE.crt \
      $certs/GoodCACert.crt $certs/TrustAnchorRootCertificate.crt
}

pkits_NameChaining()
{
  echo "***************************************************************"
  echo "Section 4.3: Verifying NameChaining"
  echo "***************************************************************"

  VFY_ACTION="Invalid Name Chaining EE Test1"
  pkitsn $certs/InvalidNameChainingTest1EE.crt \
      $certs/GoodCACert.crt $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid Name Chaining Order Test2"
  pkitsn $certs/InvalidNameChainingOrderTest2EE.crt \
      $certs/NameOrderingCACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid Name Chaining Whitespace Test3"
  pkits $certs/ValidNameChainingWhitespaceTest3EE.crt \
      $certs/GoodCACert.crt $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid Name Chaining Whitespace Test4"
  pkits $certs/ValidNameChainingWhitespaceTest4EE.crt \
      $certs/GoodCACert.crt $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid Name Chaining Capitalization Test5"
  pkits $certs/ValidNameChainingCapitalizationTest5EE.crt \
      $certs/GoodCACert.crt $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid Name Chaining UIDs Test6"
  pkits $certs/ValidNameUIDsTest6EE.crt $certs/UIDCACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid RFC3280 Mandatory Attribute Types Test7"
  pkits $certs/ValidRFC3280MandatoryAttributeTypesTest7EE.crt \
      $certs/RFC3280MandatoryAttributeTypesCACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid RFC3280 Optional Attribute Types Test8"
  pkits $certs/ValidRFC3280OptionalAttributeTypesTest8EE.crt \
      $certs/RFC3280OptionalAttributeTypesCACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid UTF8String Encoded Names Test9"
  pkits $certs/ValidUTF8StringEncodedNamesTest9EE.crt \
      $certs/UTF8StringEncodedNamesCACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid Rollover from PrintableString to UTF8String Test10"
  pkits $certs/ValidRolloverfromPrintableStringtoUTF8StringTest10EE.crt \
      $certs/RolloverfromPrintableStringtoUTF8StringCACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid UTF8String case Insensitive Match Test11"
  pkits $certs/ValidUTF8StringCaseInsensitiveMatchTest11EE.crt \
      $certs/UTF8StringCaseInsensitiveMatchCACert.crt \
      $certs/TrustAnchorRootCertificate.crt
}

pkits_PathVerificWithSelfIssuedCerts()
{
  echo "***************************************************************"
  echo "Section 4.5: Verifying Paths with Self-Issued Certificates"
  echo "***************************************************************"

  VFY_ACTION="Valid Basic Self-Issued Old With New Test1"
  pkits $certs/ValidBasicSelfIssuedOldWithNewTest1EE.crt \
      $certs/BasicSelfIssuedNewKeyOldWithNewCACert.crt \
      $certs/BasicSelfIssuedNewKeyCACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="\nInvalid Basic Self-Issued Old With New Test2"
  pkitsn $certs/InvalidBasicSelfIssuedOldWithNewTest2EE.crt \
      $certs/BasicSelfIssuedNewKeyOldWithNewCACert.crt \
      $certs/BasicSelfIssuedNewKeyCACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="\nValid Basic Self-Issued New With Old Test3"
  pkits $certs/ValidBasicSelfIssuedNewWithOldTest3EE.crt \
      $certs/BasicSelfIssuedOldKeyNewWithOldCACert.crt \
      $certs/BasicSelfIssuedOldKeyCACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid Basic Self-Issued New With Old Test4"
  pkits $certs/ValidBasicSelfIssuedNewWithOldTest4EE.crt \
      $certs/BasicSelfIssuedOldKeyNewWithOldCACert.crt \
      $certs/BasicSelfIssuedOldKeyCACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid Basic Self-Issued New With Old Test5"
  pkitsn $certs/InvalidBasicSelfIssuedNewWithOldTest5EE.crt \
      $certs/BasicSelfIssuedOldKeyNewWithOldCACert.crt \
      $certs/BasicSelfIssuedOldKeyCACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid Basic Self-Issued CRL Signing Key Test6"
  pkits $certs/ValidBasicSelfIssuedCRLSigningKeyTest6EE.crt \
      $certs/BasicSelfIssuedCRLSigningKeyCRLCert.crt \
      $certs/BasicSelfIssuedCRLSigningKeyCACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid Basic Self-Issued CRL Signing Key Test7"
  pkitsn $certs/InvalidBasicSelfIssuedCRLSigningKeyTest7EE.crt \
      $certs/BasicSelfIssuedCRLSigningKeyCRLCert.crt \
      $certs/BasicSelfIssuedCRLSigningKeyCACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid Basic Self-Issued CRL Signing Key Test8"
  pkitsn $certs/InvalidBasicSelfIssuedCRLSigningKeyTest8EE.crt \
      $certs/BasicSelfIssuedCRLSigningKeyCRLCert.crt \
      $certs/BasicSelfIssuedCRLSigningKeyCACert.crt \
      $certs/TrustAnchorRootCertificate.crt
}

pkits_BasicConstraints()
{
  echo "***************************************************************"
  echo "Section 4.6: Verifying Basic Constraints"
  echo "***************************************************************"

  VFY_ACTION="Invalid Missing basicConstraints Test1"
  pkitsn $certs/InvalidMissingbasicConstraintsTest1EE.crt \
      $certs/MissingbasicConstraintsCACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid cA False Test2"
  pkitsn $certs/InvalidcAFalseTest2EE.crt \
      $certs/basicConstraintsCriticalcAFalseCACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid cA False Test3"
  pkitsn $certs/InvalidcAFalseTest3EE.crt \
      $certs/basicConstraintsNotCriticalcAFalseCACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid basicConstraints Not Critical Test4"
  pkits $certs/ValidbasicConstraintsNotCriticalTest4EE.crt \
      $certs/basicConstraintsNotCriticalCACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid pathLenConstraint Test5"
  pkitsn $certs/InvalidpathLenConstraintTest5EE.crt \
      $certs/pathLenConstraint0subCACert.crt \
      $certs/pathLenConstraint0CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid pathLenConstraint Test6"
  pkitsn $certs/InvalidpathLenConstraintTest6EE.crt \
      $certs/pathLenConstraint0subCACert.crt \
      $certs/pathLenConstraint0CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid pathLenConstraint Test7"
  pkits $certs/ValidpathLenConstraintTest7EE.crt \
      $certs/pathLenConstraint0CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid pathLenConstraint test8"
  pkits $certs/ValidpathLenConstraintTest8EE.crt \
      $certs/pathLenConstraint0CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid pathLenConstraint Test9"
  pkitsn $certs/InvalidpathLenConstraintTest9EE.crt \
      $certs/pathLenConstraint6subsubCA00Cert.crt \
      $certs/pathLenConstraint6subCA0Cert.crt \
      $certs/pathLenConstraint6CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid pathLenConstraint Test10"
  pkitsn $certs/InvalidpathLenConstraintTest10EE.crt \
      $certs/pathLenConstraint6subsubCA00Cert.crt \
      $certs/pathLenConstraint6subCA0Cert.crt \
      $certs/pathLenConstraint6CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid pathLenConstraint Test11"
  pkitsn $certs/InvalidpathLenConstraintTest11EE.crt \
      $certs/pathLenConstraint6subsubsubCA11XCert.crt \
      $certs/pathLenConstraint6subsubCA11Cert.crt \
      $certs/pathLenConstraint6subCA1Cert.crt \
      $certs/pathLenConstraint6CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid pathLenConstraint test12"
  pkitsn $certs/InvalidpathLenConstraintTest12EE.crt \
      $certs/pathLenConstraint6subsubsubCA11XCert.crt \
      $certs/pathLenConstraint6subsubCA11Cert.crt \
      $certs/pathLenConstraint6subCA1Cert.crt \
      $certs/pathLenConstraint6CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid pathLenConstraint Test13"
  pkits $certs/ValidpathLenConstraintTest13EE.crt \
      $certs/pathLenConstraint6subsubsubCA41XCert.crt \
      $certs/pathLenConstraint6subsubCA41Cert.crt \
      $certs/pathLenConstraint6subCA4Cert.crt \
      $certs/pathLenConstraint6CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid pathLenConstraint Test14"
  pkits $certs/ValidpathLenConstraintTest14EE.crt \
      $certs/pathLenConstraint6subsubsubCA41XCert.crt \
      $certs/pathLenConstraint6subsubCA41Cert.crt \
      $certs/pathLenConstraint6subCA4Cert.crt \
      $certs/pathLenConstraint6CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid Self-Issued pathLenConstraint Test15"
  pkits $certs/ValidSelfIssuedpathLenConstraintTest15EE.crt \
      $certs/pathLenConstraint0SelfIssuedCACert.crt \
      $certs/pathLenConstraint0CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid Self-Issued pathLenConstraint Test16"
  pkitsn $certs/InvalidSelfIssuedpathLenConstraintTest16EE.crt \
      $certs/pathLenConstraint0subCA2Cert.crt \
      $certs/pathLenConstraint0SelfIssuedCACert.crt \
      $certs/pathLenConstraint0CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid Self-Issued pathLenConstraint Test17"
  pkits $certs/ValidSelfIssuedpathLenConstraintTest17EE.crt \
      $certs/pathLenConstraint1SelfIssuedsubCACert.crt \
      $certs/pathLenConstraint1subCACert.crt \
      $certs/pathLenConstraint1SelfIssuedCACert.crt \
      $certs/pathLenConstraint1CACert.crt \
      $certs/TrustAnchorRootCertificate.crt
}

pkits_NameConstraints()
{
  echo "***************************************************************"
  echo "Section 4.13: Name Constraints"
  echo "***************************************************************"

  VFY_ACTION="Valid DN nameConstraints Test1"
  pkits $certs/ValidDNnameConstraintsTest1EE.crt \
      $certs/nameConstraintsDN1CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid DN nameConstraints Test2"
  pkitsn $certs/InvalidDNnameConstraintsTest2EE.crt \
      $certs/nameConstraintsDN1CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid DN nameConstraints Test3"
  pkitsn $certs/InvalidDNnameConstraintsTest3EE.crt \
      $certs/nameConstraintsDN1CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid DN nameConstraints Test4"
  pkits $certs/ValidDNnameConstraintsTest4EE.crt \
      $certs/nameConstraintsDN1CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid DN nameConstraints Test5"
  pkits $certs/ValidDNnameConstraintsTest5EE.crt \
      $certs/nameConstraintsDN2CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid DN nameConstraints Test6"
  pkits $certs/ValidDNnameConstraintsTest6EE.crt \
      $certs/nameConstraintsDN3CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid DN nameConstraints Test7"
  pkitsn $certs/InvalidDNnameConstraintsTest7EE.crt \
      $certs/nameConstraintsDN3CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid DN nameConstraints Test8"
  pkitsn $certs/InvalidDNnameConstraintsTest8EE.crt \
      $certs/nameConstraintsDN4CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid DN nameConstraints Test9"
  pkitsn $certs/InvalidDNnameConstraintsTest9EE.crt \
      $certs/nameConstraintsDN4CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid DN nameConstraints Test10"
  pkitsn $certs/InvalidDNnameConstraintsTest10EE.crt \
      $certs/nameConstraintsDN5CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid DN nameConstraints Test11"
  pkits $certs/ValidDNnameConstraintsTest11EE.crt \
      $certs/nameConstraintsDN5CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid DN nameConstraints Test12"
  pkitsn $certs/InvalidDNnameConstraintsTest12EE.crt \
      $certs/nameConstraintsDN1subCA1Cert.crt \
      $certs/nameConstraintsDN1CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid DN nameConstraints Test13"
  pkitsn $certs/InvalidDNnameConstraintsTest13EE.crt \
      $certs/nameConstraintsDN1subCA2Cert.crt \
      $certs/nameConstraintsDN1CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid DN nameConstraints Test14"
  pkits $certs/ValidDNnameConstraintsTest14EE.crt \
      $certs/nameConstraintsDN1subCA2Cert.crt \
      $certs/nameConstraintsDN1CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid DN nameConstraints Test15"
  pkitsn $certs/InvalidDNnameConstraintsTest15EE.crt \
      $certs/nameConstraintsDN3subCA1Cert.crt \
      $certs/nameConstraintsDN3CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid DN nameConstraints Test16"
  pkitsn $certs/InvalidDNnameConstraintsTest16EE.crt \
      $certs/nameConstraintsDN3subCA1Cert.crt \
      $certs/nameConstraintsDN3CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid DN nameConstraints Test17"
  pkitsn $certs/InvalidDNnameConstraintsTest17EE.crt \
      $certs/nameConstraintsDN3subCA2Cert.crt \
      $certs/nameConstraintsDN3CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid DN nameConstraints Test18"
  pkits $certs/ValidDNnameConstraintsTest18EE.crt \
      $certs/nameConstraintsDN3subCA2Cert.crt \
      $certs/nameConstraintsDN3CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid Self-Issued DN nameConstraints Test19"
  pkits certs/ValidDNnameConstraintsTest19EE.crt \
      $certs/nameConstraintsDN1SelfIssuedCACert.crt \
      $certs/nameConstraintsDN1CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid Self-Issued DN nameConstraints Test20"
  pkitsn $certs/InvalidDNnameConstraintsTest20EE.crt \
      $certs/nameConstraintsDN1CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid RFC822 nameConstraints Test21"
  pkits $certs/ValidRFC822nameConstraintsTest21EE.crt \
      $certs/nameConstraintsRFC822CA1Cert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid RFC822 nameConstraints Test22"
  pkitsn $certs/InvalidRFC822nameConstraintsTest22EE.crt \
      $certs/nameConstraintsRFC822CA1Cert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid RFC822 nameConstraints Test23"
  pkits $certs/ValidRFC822nameConstraintsTest23EE.crt \
      $certs/nameConstraintsRFC822CA2Cert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid RFC822 nameConstraints Test24"
  pkitsn $certs/InvalidRFC822nameConstraintsTest24EE.crt \
      $certs/nameConstraintsRFC822CA2Cert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid RFC822 nameConstraints Test25"
  pkits $certs/ValidRFC822nameConstraintsTest25EE.crt \
      $certs/nameConstraintsRFC822CA3Cert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid RFC822 nameConstraints Test26"
  pkitsn $certs/InvalidRFC822nameConstraintsTest26EE.crt \
      $certs/nameConstraintsRFC822CA3Cert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid DN and RFC822 nameConstraints Test27"
  pkits $certs/ValidDNandRFC822nameConstraintsTest27EE.crt \
      $certs/nameConstraintsDN1subCA3Cert.crt \
      $certs/nameConstraintsDN1CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid DN and RFC822 nameConstraints Test28"
  pkitsn $certs/InvalidDNandRFC822nameConstraintsTest28EE.crt \
      $certs/nameConstraintsDN1subCA3Cert.crt \
      $certs/nameConstraintsDN1CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid DN and RFC822 nameConstraints Test29"
  pkitsn $certs/InvalidDNandRFC822nameConstraintsTest29EE.crt \
      $certs/nameConstraintsDN1subCA3Cert.crt \
      $certs/nameConstraintsDN1CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid DNS nameConstraints Test30"
  pkits $certs/ValidDNSnameConstraintsTest30EE.crt \
      $certs/nameConstraintsDNS1CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid DNS nameConstraints Test31"
  pkitsn $certs/InvalidDNSnameConstraintsTest31EE.crt \
      $certs/nameConstraintsDNS1CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid DNS nameConstraints Test32"
  pkits $certs/ValidDNSnameConstraintsTest32EE.crt \
      $certs/nameConstraintsDNS2CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid DNS nameConstraints Test33"
  pkitsn $certs/InvalidDNSnameConstraintsTest33EE.crt \
      $certs/nameConstraintsDNS2CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid URI nameConstraints Test34"
  pkits $certs/ValidURInameConstraintsTest34EE.crt \
      $certs/nameConstraintsURI1CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid URI nameConstraints Test35"
  pkitsn $certs/InvalidURInameConstraintsTest35EE.crt \
      $certs/nameConstraintsURI1CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Valid URI nameConstraints Test36"
  pkits $certs/ValidURInameConstraintsTest36EE.crt \
      $certs/nameConstraintsURI2CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid URI nameConstraints Test37"
  pkitsn $certs/InvalidURInameConstraintsTest37EE.crt \
      $certs/nameConstraintsURI2CACert.crt \
      $certs/TrustAnchorRootCertificate.crt

  VFY_ACTION="Invalid DNS nameConstraints Test38"
  pkitsn $certs/InvalidDNSnameConstraintsTest38EE.crt \
      $certs/nameConstraintsDNS1CACert.crt \
      $certs/TrustAnchorRootCertificate.crt
}


############################## pkits_cleanup ###########################
# local shell function to finish this script (no exit since it might be 
# sourced)
########################################################################
pkits_cleanup()
{
  html "</TABLE><BR>"
  cd ${QADIR}
  . common/cleanup.sh
}


################################## main ################################
pkits_init
pkits_SignatureVerification | tee $PKITS_LOG
pkits_ValidityPeriods | tee -a $PKITS_LOG
pkits_NameChaining | tee -a $PKITS_LOG
pkits_PathVerificWithSelfIssuedCerts | tee -a $PKITS_LOG
pkits_BasicConstraints | tee -a $PKITS_LOG
pkits_NameConstraints | tee -a $PKITS_LOG
pkits_cleanup
