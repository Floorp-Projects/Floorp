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
  pkits_log "Start of logfile $PKITS_LOG"

  if [ ! -d "${PKITSdb}" ]; then
      mkdir -p ${PKITSdb}
  else
      pkits_log "$SCRIPTNAME: WARNING - ${PKITSdb} exists"
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
  if [ "$NSS_NO_PKITS_CRLS" -ne 1 ]; then
    crlutil -I -i $crls/TrustAnchorRootCRL.crl -d ${PKITSdb}
  else
    html  "<H3>NO CRLs are being used.</H3>"
    pkits_log "NO CRLs are being used."
  fi
}

############################### pkits_log ##############################
# write to pkits.log file
########################################################################
pkits_log()
{
  echo "$SCRIPTNAME $*"
  echo $* >> ${PKITS_LOG}
}

log_banner()
{
  echo ""
  echo "--------------------------------------------------------------------"
  echo "Test case ${VFY_ACTION}"
  echo ""
}

start_table()
{
  html "<TABLE BORDER=1><TR><TH COLSPAN=3>$*</TH></TR>"
  html "<TR><TH width=500>Test Case</TH><TH width=50>Result</TH></TR>" 
  echo ""
  echo "***************************************************************"
  echo "$*"
  echo "***************************************************************"
}

break_table()
{
  html "</TABLE><P>"
  start_table "$@"
}

################################ pkits #################################
# local shell function for positive testcases, calls vfychain, writes 
# action and options to stdout, sets variable RET and writes results to 
# the html file results
########################################################################
pkits()
{
  echo "vfychain -d $PKITSdb -u 4 $*"
  vfychain -d $PKITSdb -u 4 $* > ${PKITSDIR}/cmdout.txt 2>&1
  RET=$?
  RET=$(expr $RET + $(grep -c ERROR ${PKITSDIR}/cmdout.txt))
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
  echo "vfychain -d $PKITSdb -u 4 $*"
  vfychain -d $PKITSdb -u 4 $* > ${PKITSDIR}/cmdout.txt 2>&1
  RET=$?
  RET=$(expr $RET + $(grep -c ERROR ${PKITSDIR}/cmdout.txt))
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

################################ crlImport #############################
# local shell function to import a CRL, calls crlutil -I -i, writes 
# action and options to stdout
########################################################################
crlImport()
{
  if [ "$NSS_NO_PKITS_CRLS" -ne 1 ]; then
    echo "crlutil -d $PKITSdb -I -i $crls/$*"
    crlutil -d ${PKITSdb} -I -i $crls/$* > ${PKITSDIR}/cmdout.txt 2>&1
    RET=$?
    cat ${PKITSDIR}/cmdout.txt

    if [ "$RET" -ne 0 ]; then
	html_failed "<TR><TD>${VFY_ACTION} ($RET) "
	pkits_log "ERROR: ${VFY_ACTION} failed $RET"
    fi
  fi
}

################################ crlImportn #############################
# local shell function to import an incorrect CRL, calls crlutil -I -i, 
# writes action and options to stdout
########################################################################
crlImportn()
{
  RET=0
  if [ "$NSS_NO_PKITS_CRLS" -ne 1 ]; then
    echo "crlutil -d $PKITSdb -I -i $crls/$*"
    crlutil -d ${PKITSdb} -I -i $crls/$* > ${PKITSDIR}/cmdout.txt 2>&1
    RET=$?
    cat ${PKITSDIR}/cmdout.txt

    if [ "$RET" -eq 0 ]; then
	html_failed "<TR><TD>${VFY_ACTION} ($RET) "
	pkits_log "ERROR: ${VFY_ACTION} failed $RET"
    else
	html_passed "<TR><TD>${VFY_ACTION} ($RET) "
	pkits_log "SUCCESS: ${VFY_ACTION} returned as expected $RET"
    fi
  fi
  return $RET
}

############################### delete #############################
# local shell function to delete a CRL, calls crlutil -D -n, writes 
# action and options to stdout
# then deletes the associated cert, calls certutil -D -n, writes 
# action and options to stdout
########################################################################
delete()
{
  if [ "$NSS_NO_PKITS_CRLS" -ne 1 ]; then
      echo "crlutil -d $PKITSdb -D -n $*"
      crlutil -d ${PKITSdb} -D -n $* > ${PKITSDIR}/cmdout.txt 2>&1
      RET=$?
      cat ${PKITSDIR}/cmdout.txt

      if [ "$RET" -ne 0 ]; then
	  html_failed "<TR><TD>${VFY_ACTION} ($RET) "
	  pkits_log "ERROR: ${VFY_ACTION} failed $RET"
      fi
  fi

  echo "certutil -d $PKITSdb -D -n $*"
  certutil -d $PKITSdb -D -n $* > ${PKITSDIR}/cmdout.txt 2>&1
  RET=$?
  cat ${PKITSDIR}/cmdout.txt

  if [ "$RET" -ne 0 ]; then
      html_failed "<TR><TD>${VFY_ACTION} ($RET) "
      pkits_log "ERROR: ${VFY_ACTION} failed $RET"
  fi
}

################################ certImport #############################
# local shell function to import a Cert, calls certutil -A, writes 
# action and options to stdout
########################################################################
certImport()
{
  echo "certutil -d $PKITSdb -A -t \",,\" -n $* -i $certs/$*.crt"
  certutil -d $PKITSdb -A -t ",," -n $* -i $certs/$*.crt > ${PKITSDIR}/cmdout.txt 2>&1
  RET=$?
  cat ${PKITSDIR}/cmdout.txt

  if [ "$RET" -ne 0 ]; then
      html_failed "<TR><TD>${VFY_ACTION} ($RET) "
      pkits_log "ERROR: ${VFY_ACTION} failed $RET"
  fi
}

############################## pkits_tests_bySection ###################
# running the various PKITS tests
########################################################################
pkits_SignatureVerification()
{
  start_table "NIST PKITS Section 4.1: Signature Verification"

  VFY_ACTION="Valid Signatures Test1"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkits $certs/ValidCertificatePathTest1EE.crt $certs/GoodCACert.crt
  delete GoodCACert

  VFY_ACTION="Invalid CA Signature Test2"; log_banner
  certImport BadSignedCACert
  crlImport BadSignedCACRL.crl
  pkitsn $certs/InvalidCASignatureTest2EE.crt \
    $certs/BadSignedCACert.crt
  delete BadSignedCACert

  VFY_ACTION="Invalid EE Signature Test3"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkitsn $certs/InvalidEESignatureTest3EE.crt $certs/GoodCACert.crt
  delete GoodCACert

  VFY_ACTION="Valid DSA Signatures Test4"; log_banner
  certImport DSACACert
  crlImport DSACACRL.crl
  pkits $certs/ValidDSASignaturesTest4EE.crt $certs/DSACACert.crt
  delete DSACACert

  VFY_ACTION="Valid DSA Parameter Inheritance Test5"; log_banner
  certImport DSACACert
  crlImport DSACACRL.crl
  certImport DSAParametersInheritedCACert
  crlImport DSAParametersInheritedCACRL.crl
  pkits $certs/ValidDSAParameterInheritanceTest5EE.crt \
      $certs/DSAParametersInheritedCACert.crt \
      $certs/DSACACert.crt
  delete DSAParametersInheritedCACert
  delete DSACACert

  VFY_ACTION="Invalid DSA Signature Test6"; log_banner
  certImport DSACACert
  crlImport DSACACRL.crl
  pkitsn $certs/InvalidDSASignatureTest6EE.crt $certs/DSACACert.crt
  delete DSACACert
}

pkits_ValidityPeriods()
{
  break_table "NIST PKITS Section 4.2: Validity Periods"

  VFY_ACTION="Invalid CA notBefore Date Test1"; log_banner
  certImport BadnotBeforeDateCACert
  crlImportn BadnotBeforeDateCACRL.crl
  if [ $RET -eq 0 ] ; then 
      pkitsn $certs/InvalidCAnotBeforeDateTest1EE.crt \
	  $certs/BadnotBeforeDateCACert.crt
  fi
  delete BadnotBeforeDateCACert

  VFY_ACTION="Invalid EE notBefore Date Test2"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkitsn $certs/InvalidEEnotBeforeDateTest2EE.crt \
      $certs/GoodCACert.crt
  delete GoodCACert

  VFY_ACTION="Valid pre2000 UTC notBefore Date Test3"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkits $certs/Validpre2000UTCnotBeforeDateTest3EE.crt \
      $certs/GoodCACert.crt
  delete GoodCACert

  VFY_ACTION="Valid GeneralizedTime notBefore Date Test4"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkits $certs/ValidGeneralizedTimenotBeforeDateTest4EE.crt \
      $certs/GoodCACert.crt
  delete GoodCACert

  VFY_ACTION="Invalid CA notAfter Date Test5"; log_banner
  certImport BadnotAfterDateCACert
  crlImportn BadnotAfterDateCACRL.crl
  if [ $RET -eq 0 ] ; then 
      pkitsn $certs/InvalidCAnotAfterDateTest5EE.crt \
	  $certs/BadnotAfterDateCACert.crt
  fi
  delete BadnotAfterDateCACert

  VFY_ACTION="Invalid EE notAfter Date Test6"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkitsn $certs/InvalidEEnotAfterDateTest6EE.crt \
      $certs/GoodCACert.crt
  delete GoodCACert

  VFY_ACTION="Invalid pre2000 UTC EE notAfter Date Test7"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkitsn $certs/Invalidpre2000UTCEEnotAfterDateTest7EE.crt \
      $certs/GoodCACert.crt
  delete GoodCACert

  VFY_ACTION="ValidGeneralizedTime notAfter Date Test8"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkits $certs/ValidGeneralizedTimenotAfterDateTest8EE.crt \
      $certs/GoodCACert.crt
  delete GoodCACert
}

pkits_NameChaining()
{
  break_table "NIST PKITS Section 4.3: Verifying NameChaining"

  VFY_ACTION="Invalid Name Chaining EE Test1"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkitsn $certs/InvalidNameChainingTest1EE.crt \
      $certs/GoodCACert.crt
  delete GoodCACert

  VFY_ACTION="Invalid Name Chaining Order Test2"; log_banner
  certImport NameOrderingCACert
  crlImport NameOrderCACRL.crl
  pkitsn $certs/InvalidNameChainingOrderTest2EE.crt \
      $certs/NameOrderingCACert.crt
  delete NameOrderingCACert

  VFY_ACTION="Valid Name Chaining Whitespace Test3"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkits $certs/ValidNameChainingWhitespaceTest3EE.crt \
      $certs/GoodCACert.crt
  delete GoodCACert

  VFY_ACTION="Valid Name Chaining Whitespace Test4"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkits $certs/ValidNameChainingWhitespaceTest4EE.crt \
      $certs/GoodCACert.crt
  delete GoodCACert

  VFY_ACTION="Valid Name Chaining Capitalization Test5"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkits $certs/ValidNameChainingCapitalizationTest5EE.crt \
      $certs/GoodCACert.crt
  delete GoodCACert

  VFY_ACTION="Valid Name Chaining UIDs Test6"; log_banner
  certImport UIDCACert
  crlImport UIDCACRL.crl
  pkits $certs/ValidNameUIDsTest6EE.crt $certs/UIDCACert.crt
  delete UIDCACert

  VFY_ACTION="Valid RFC3280 Mandatory Attribute Types Test7"; log_banner
  certImport RFC3280MandatoryAttributeTypesCACert
  crlImport RFC3280MandatoryAttributeTypesCACRL.crl
  pkits $certs/ValidRFC3280MandatoryAttributeTypesTest7EE.crt \
      $certs/RFC3280MandatoryAttributeTypesCACert.crt
  delete RFC3280MandatoryAttributeTypesCACert

  VFY_ACTION="Valid RFC3280 Optional Attribute Types Test8"; log_banner
  certImport RFC3280OptionalAttributeTypesCACert
  crlImport RFC3280OptionalAttributeTypesCACRL.crl
  pkits $certs/ValidRFC3280OptionalAttributeTypesTest8EE.crt \
      $certs/RFC3280OptionalAttributeTypesCACert.crt
  delete RFC3280OptionalAttributeTypesCACert

  VFY_ACTION="Valid UTF8String Encoded Names Test9"; log_banner
  certImport UTF8StringEncodedNamesCACert
  crlImport UTF8StringEncodedNamesCACRL.crl
  pkits $certs/ValidUTF8StringEncodedNamesTest9EE.crt \
      $certs/UTF8StringEncodedNamesCACert.crt
  delete UTF8StringEncodedNamesCACert

  VFY_ACTION="Valid Rollover from PrintableString to UTF8String Test10"; log_banner
  certImport RolloverfromPrintableStringtoUTF8StringCACert
  crlImport RolloverfromPrintableStringtoUTF8StringCACRL.crl
  pkits $certs/ValidRolloverfromPrintableStringtoUTF8StringTest10EE.crt \
      $certs/RolloverfromPrintableStringtoUTF8StringCACert.crt
  delete RolloverfromPrintableStringtoUTF8StringCACert

  VFY_ACTION="Valid UTF8String case Insensitive Match Test11"; log_banner
  certImport UTF8StringCaseInsensitiveMatchCACert
  crlImport UTF8StringCaseInsensitiveMatchCACRL.crl
  pkits $certs/ValidUTF8StringCaseInsensitiveMatchTest11EE.crt \
      $certs/UTF8StringCaseInsensitiveMatchCACert.crt
  delete UTF8StringCaseInsensitiveMatchCACert
}

pkits_BasicCertRevocation()
{
  break_table "NIST PKITS Section 4.4: Basic Certificate Revocation Tests"

  VFY_ACTION="Missing CRL Test1"; log_banner
  pkitsn $certs/InvalidMissingCRLTest1EE.crt \
      $certs/NoCRLCACert.crt

  VFY_ACTION="Invalid Revoked CA Test2"; log_banner
  certImport RevokedsubCACert
  crlImport RevokedsubCACRL.crl
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkitsn $certs/InvalidRevokedCATest2EE.crt \
     $certs/RevokedsubCACert.crt $certs/GoodCACert.crt
  delete RevokedsubCACert
  delete GoodCACert

  VFY_ACTION="Invalid Revoked EE Test3"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkitsn $certs/InvalidRevokedEETest3EE.crt \
     $certs/GoodCACert.crt
  delete GoodCACert

  VFY_ACTION="Invalid Bad CRL Signature Test4"; log_banner
  certImport BadCRLSignatureCACert
  crlImportn BadCRLSignatureCACRL.crl
  if [ $RET -eq 0 ] ; then 
      pkitsn $certs/InvalidBadCRLSignatureTest4EE.crt \
	 $certs/BadCRLSignatureCACert.crt
  fi
  delete BadCRLSignatureCACert

  VFY_ACTION="Invalid Bad CRL Issuer Name Test5"; log_banner
  certImport BadCRLIssuerNameCACert
  crlImportn BadCRLIssuerNameCACRL.crl
  if [ $RET -eq 0 ] ; then 
      pkitsn $certs/InvalidBadCRLIssuerNameTest5EE.crt \
	 $certs/BadCRLIssuerNameCACert.crt
  fi
  delete BadCRLIssuerNameCACert

  VFY_ACTION="Invalid Wrong CRL Test6"; log_banner
  certImport WrongCRLCACert
  crlImportn WrongCRLCACRL.crl
  if [ $RET -eq 0 ] ; then 
      pkitsn $certs/InvalidWrongCRLTest6EE.crt \
	 $certs/WrongCRLCACert.crt
  fi
  delete WrongCRLCACert

  VFY_ACTION="Valid Two CRLs Test7"; log_banner
  certImport TwoCRLsCACert
  crlImport TwoCRLsCAGoodCRL.crl
  crlImportn TwoCRLsCABadCRL.crl
  pkits $certs/ValidTwoCRLsTest7EE.crt \
     $certs/TwoCRLsCACert.crt
  delete TwoCRLsCACert

  VFY_ACTION="Invalid Unknown CRL Entry Extension Test8"; log_banner
  certImport UnknownCRLEntryExtensionCACert
  crlImportn UnknownCRLEntryExtensionCACRL.crl
  if [ $RET -eq 0 ] ; then 
      pkitsn $certs/InvalidUnknownCRLEntryExtensionTest8EE.crt \
	 $certs/UnknownCRLEntryExtensionCACert.crt
  fi
  delete UnknownCRLEntryExtensionCACert

  VFY_ACTION="Invalid Unknown CRL Extension Test9"; log_banner
  certImport UnknownCRLExtensionCACert
  crlImportn UnknownCRLExtensionCACRL.crl
  if [ $RET -eq 0 ] ; then 
      pkitsn $certs/InvalidUnknownCRLExtensionTest9EE.crt \
	 $certs/UnknownCRLExtensionCACert.crt
  fi
  delete UnknownCRLExtensionCACert

  VFY_ACTION="Invalid Unknown CRL Extension Test10"; log_banner
  certImport UnknownCRLExtensionCACert
  crlImportn UnknownCRLExtensionCACRL.crl
  if [ $RET -eq 0 ] ; then 
      pkitsn $certs/InvalidUnknownCRLExtensionTest10EE.crt \
	 $certs/UnknownCRLExtensionCACert.crt
  fi
  delete UnknownCRLExtensionCACert

  VFY_ACTION="Invalid Old CRL nextUpdate Test11"; log_banner
  certImport OldCRLnextUpdateCACert
  crlImport OldCRLnextUpdateCACRL.crl
  pkitsn $certs/InvalidOldCRLnextUpdateTest11EE.crt \
     $certs/OldCRLnextUpdateCACert.crt
  delete OldCRLnextUpdateCACert

  VFY_ACTION="Invalid pre2000 CRL nextUpdate Test12"; log_banner
  certImport pre2000CRLnextUpdateCACert
  crlImport pre2000CRLnextUpdateCACRL.crl
  pkitsn $certs/Invalidpre2000CRLnextUpdateTest12EE.crt \
     $certs/pre2000CRLnextUpdateCACert.crt
  delete pre2000CRLnextUpdateCACert

  VFY_ACTION="Valid GeneralizedTime CRL nextUpdate Test13"; log_banner
  certImport GeneralizedTimeCRLnextUpdateCACert
  crlImport GeneralizedTimeCRLnextUpdateCACRL.crl
  pkits $certs/ValidGeneralizedTimeCRLnextUpdateTest13EE.crt \
     $certs/GeneralizedTimeCRLnextUpdateCACert.crt
  delete GeneralizedTimeCRLnextUpdateCACert

  VFY_ACTION="Valid Negative Serial Number Test14"; log_banner
  certImport NegativeSerialNumberCACert
  crlImport NegativeSerialNumberCACRL.crl
  pkits $certs/ValidNegativeSerialNumberTest14EE.crt \
     $certs/NegativeSerialNumberCACert.crt
  delete NegativeSerialNumberCACert

  VFY_ACTION="Invalid Negative Serial Number Test15"; log_banner
  certImport NegativeSerialNumberCACert
  crlImport NegativeSerialNumberCACRL.crl
  pkitsn $certs/InvalidNegativeSerialNumberTest15EE.crt \
     $certs/NegativeSerialNumberCACert.crt
  delete NegativeSerialNumberCACert

  VFY_ACTION="Valid Long Serial Number Test16"; log_banner
  certImport LongSerialNumberCACert
  crlImport LongSerialNumberCACRL.crl
  pkits $certs/ValidLongSerialNumberTest16EE.crt \
     $certs/LongSerialNumberCACert.crt
  delete LongSerialNumberCACert

  VFY_ACTION="Valid Long Serial Number Test17"; log_banner
  certImport LongSerialNumberCACert
  crlImport LongSerialNumberCACRL.crl
  pkits $certs/ValidLongSerialNumberTest17EE.crt \
     $certs/LongSerialNumberCACert.crt
  delete LongSerialNumberCACert

  VFY_ACTION="Invalid Long Serial Number Test18"; log_banner
  certImport LongSerialNumberCACert
  crlImport LongSerialNumberCACRL.crl
  pkitsn $certs/InvalidLongSerialNumberTest18EE.crt \
     $certs/LongSerialNumberCACert.crt
  delete LongSerialNumberCACert
}

pkits_PathVerificWithSelfIssuedCerts()
{
  break_table "NIST PKITS Section 4.5: Self-Issued Certificates"

  VFY_ACTION="Valid Basic Self-Issued Old With New Test1"; log_banner
  certImport BasicSelfIssuedNewKeyCACert
  crlImport BasicSelfIssuedNewKeyCACRL.crl
  pkits $certs/ValidBasicSelfIssuedOldWithNewTest1EE.crt \
      $certs/BasicSelfIssuedNewKeyOldWithNewCACert.crt \
      $certs/BasicSelfIssuedNewKeyCACert.crt
  delete BasicSelfIssuedNewKeyCACert

  VFY_ACTION="Invalid Basic Self-Issued Old With New Test2"; log_banner
  certImport BasicSelfIssuedNewKeyCACert
  crlImport BasicSelfIssuedNewKeyCACRL.crl
  pkitsn $certs/InvalidBasicSelfIssuedOldWithNewTest2EE.crt \
      $certs/BasicSelfIssuedNewKeyOldWithNewCACert.crt \
      $certs/BasicSelfIssuedNewKeyCACert.crt
  delete BasicSelfIssuedNewKeyCACert

  VFY_ACTION="Valid Basic Self-Issued New With Old Test3"; log_banner
  certImport BasicSelfIssuedOldKeyCACert
  crlImport BasicSelfIssuedOldKeyCACRL.crl
  pkits $certs/ValidBasicSelfIssuedNewWithOldTest3EE.crt \
      $certs/BasicSelfIssuedOldKeyNewWithOldCACert.crt \
      $certs/BasicSelfIssuedOldKeyCACert.crt
  delete BasicSelfIssuedOldKeyCACert

  VFY_ACTION="Valid Basic Self-Issued New With Old Test4"; log_banner
  certImport BasicSelfIssuedOldKeyCACert
  crlImport BasicSelfIssuedOldKeyCACRL.crl
  pkits $certs/ValidBasicSelfIssuedNewWithOldTest4EE.crt \
      $certs/BasicSelfIssuedOldKeyNewWithOldCACert.crt \
      $certs/BasicSelfIssuedOldKeyCACert.crt
  delete BasicSelfIssuedOldKeyCACert

  VFY_ACTION="Invalid Basic Self-Issued New With Old Test5"; log_banner
  certImport BasicSelfIssuedOldKeyCACert
  crlImport BasicSelfIssuedOldKeyCACRL.crl
  pkitsn $certs/InvalidBasicSelfIssuedNewWithOldTest5EE.crt \
      $certs/BasicSelfIssuedOldKeyNewWithOldCACert.crt \
      $certs/BasicSelfIssuedOldKeyCACert.crt
  delete BasicSelfIssuedOldKeyCACert

  VFY_ACTION="Valid Basic Self-Issued CRL Signing Key Test6"; log_banner
  certImport BasicSelfIssuedCRLSigningKeyCACert
  crlImport BasicSelfIssuedOldKeyCACRL.crl
  pkits $certs/ValidBasicSelfIssuedCRLSigningKeyTest6EE.crt \
      $certs/BasicSelfIssuedCRLSigningKeyCRLCert.crt \
      $certs/BasicSelfIssuedCRLSigningKeyCACert.crt
  delete BasicSelfIssuedCRLSigningKeyCACert

  VFY_ACTION="Invalid Basic Self-Issued CRL Signing Key Test7"; log_banner
  certImport BasicSelfIssuedCRLSigningKeyCACert
  crlImport BasicSelfIssuedOldKeyCACRL.crl
  pkitsn $certs/InvalidBasicSelfIssuedCRLSigningKeyTest7EE.crt \
      $certs/BasicSelfIssuedCRLSigningKeyCRLCert.crt \
      $certs/BasicSelfIssuedCRLSigningKeyCACert.crt
  delete BasicSelfIssuedCRLSigningKeyCACert

  VFY_ACTION="Invalid Basic Self-Issued CRL Signing Key Test8"; log_banner
  certImport BasicSelfIssuedCRLSigningKeyCACert
  crlImport BasicSelfIssuedOldKeyCACRL.crl
  pkitsn $certs/InvalidBasicSelfIssuedCRLSigningKeyTest8EE.crt \
      $certs/BasicSelfIssuedCRLSigningKeyCRLCert.crt \
      $certs/BasicSelfIssuedCRLSigningKeyCACert.crt
  delete BasicSelfIssuedCRLSigningKeyCACert
}

pkits_BasicConstraints()
{
  break_table "NIST PKITS Section 4.6: Verifying Basic Constraints"

  VFY_ACTION="Invalid Missing basicConstraints Test1"; log_banner
  certImport MissingbasicConstraintsCACert
  crlImport MissingbasicConstraintsCACRL.crl
  pkitsn $certs/InvalidMissingbasicConstraintsTest1EE.crt \
      $certs/MissingbasicConstraintsCACert.crt
  delete MissingbasicConstraintsCACert

  VFY_ACTION="Invalid cA False Test2"; log_banner
  certImport basicConstraintsCriticalcAFalseCACert
  crlImport basicConstraintsCriticalcAFalseCACRL.crl
  pkitsn $certs/InvalidcAFalseTest2EE.crt \
      $certs/basicConstraintsCriticalcAFalseCACert.crt
  delete basicConstraintsCriticalcAFalseCACert

  VFY_ACTION="Invalid cA False Test3"; log_banner
  certImport basicConstraintsNotCriticalcAFalseCACert
  crlImport basicConstraintsNotCriticalcAFalseCACRL.crl
  pkitsn $certs/InvalidcAFalseTest3EE.crt \
      $certs/basicConstraintsNotCriticalcAFalseCACert.crt
  delete basicConstraintsNotCriticalcAFalseCACert

  VFY_ACTION="Valid basicConstraints Not Critical Test4"; log_banner
  certImport basicConstraintsNotCriticalCACert
  crlImport basicConstraintsNotCriticalCACRL.crl
  pkits $certs/ValidbasicConstraintsNotCriticalTest4EE.crt \
      $certs/basicConstraintsNotCriticalCACert.crt
  delete basicConstraintsNotCriticalCACert

  VFY_ACTION="Invalid pathLenConstraint Test5"; log_banner
  certImport pathLenConstraint0CACert
  crlImport pathLenConstraint0CACRL.crl
  certImport pathLenConstraint0subCACert
  crlImport pathLenConstraint0subCACRL.crl
  pkitsn $certs/InvalidpathLenConstraintTest5EE.crt \
      $certs/pathLenConstraint0subCACert.crt \
      $certs/pathLenConstraint0CACert.crt
  delete pathLenConstraint0CACert
  delete pathLenConstraint0subCACert

  VFY_ACTION="Invalid pathLenConstraint Test6"; log_banner
  certImport pathLenConstraint0CACert
  crlImport pathLenConstraint0CACRL.crl
  certImport pathLenConstraint0subCACert
  crlImport pathLenConstraint0subCACRL.crl
  pkitsn $certs/InvalidpathLenConstraintTest6EE.crt \
      $certs/pathLenConstraint0subCACert.crt \
      $certs/pathLenConstraint0CACert.crt
  delete pathLenConstraint0CACert
  delete pathLenConstraint0subCACert

  VFY_ACTION="Valid pathLenConstraint Test7"; log_banner
  certImport pathLenConstraint0CACert
  crlImport pathLenConstraint0CACRL.crl
  pkits $certs/ValidpathLenConstraintTest7EE.crt \
      $certs/pathLenConstraint0CACert.crt
  delete pathLenConstraint0CACert

  VFY_ACTION="Valid pathLenConstraint test8"; log_banner
  certImport pathLenConstraint0CACert
  crlImport pathLenConstraint0CACRL.crl
  pkits $certs/ValidpathLenConstraintTest8EE.crt \
      $certs/pathLenConstraint0CACert.crt
  delete pathLenConstraint0CACert

  VFY_ACTION="Invalid pathLenConstraint Test9"; log_banner
  certImport pathLenConstraint6CACert
  crlImport pathLenConstraint6CACRL.crl
  certImport pathLenConstraint6subCA0Cert
  crlImport pathLenConstraint6subCA0CRL.crl
  certImport pathLenConstraint6subsubCA00Cert
  crlImport pathLenConstraint6subsubCA00CRL.crl
  pkitsn $certs/InvalidpathLenConstraintTest9EE.crt \
      $certs/pathLenConstraint6subsubCA00Cert.crt \
      $certs/pathLenConstraint6subCA0Cert.crt \
      $certs/pathLenConstraint6CACert.crt
  delete pathLenConstraint6CACert
  delete pathLenConstraint6subCA0Cert
  delete pathLenConstraint6subsubCA00Cert

  VFY_ACTION="Invalid pathLenConstraint Test10"; log_banner
  certImport pathLenConstraint6CACert
  crlImport pathLenConstraint6CACRL.crl
  certImport pathLenConstraint6subCA0Cert
  crlImport pathLenConstraint6subCA0CRL.crl
  certImport pathLenConstraint6subsubCA00Cert
  crlImport pathLenConstraint6subsubCA00CRL.crl
  pkitsn $certs/InvalidpathLenConstraintTest10EE.crt \
      $certs/pathLenConstraint6subsubCA00Cert.crt \
      $certs/pathLenConstraint6subCA0Cert.crt \
      $certs/pathLenConstraint6CACert.crt
  delete pathLenConstraint6CACert
  delete pathLenConstraint6subCA0Cert
  delete pathLenConstraint6subsubCA00Cert

  VFY_ACTION="Invalid pathLenConstraint Test11"; log_banner
  certImport pathLenConstraint6CACert
  crlImport pathLenConstraint6CACRL.crl
  certImport pathLenConstraint6subCA1Cert
  crlImport pathLenConstraint6subCA1CRL.crl
  certImport pathLenConstraint6subsubCA11Cert
  crlImport pathLenConstraint6subsubCA11CRL.crl
  certImport pathLenConstraint6subsubsubCA11XCert
  crlImport pathLenConstraint6subsubsubCA11XCRL.crl
  pkitsn $certs/InvalidpathLenConstraintTest11EE.crt \
      $certs/pathLenConstraint6subsubsubCA11XCert.crt \
      $certs/pathLenConstraint6subsubCA11Cert.crt \
      $certs/pathLenConstraint6subCA1Cert.crt \
      $certs/pathLenConstraint6CACert.crt
  delete pathLenConstraint6CACert
  delete pathLenConstraint6subCA1Cert
  delete pathLenConstraint6subsubCA11Cert
  delete pathLenConstraint6subsubsubCA11XCert

  VFY_ACTION="Invalid pathLenConstraint test12"; log_banner
  certImport pathLenConstraint6CACert
  crlImport pathLenConstraint6CACRL.crl
  certImport pathLenConstraint6subCA1Cert
  crlImport pathLenConstraint6subCA1CRL.crl
  certImport pathLenConstraint6subsubCA11Cert
  crlImport pathLenConstraint6subsubCA11CRL.crl
  certImport pathLenConstraint6subsubsubCA11XCert
  crlImport pathLenConstraint6subsubsubCA11XCRL.crl
  pkitsn $certs/InvalidpathLenConstraintTest12EE.crt \
      $certs/pathLenConstraint6subsubsubCA11XCert.crt \
      $certs/pathLenConstraint6subsubCA11Cert.crt \
      $certs/pathLenConstraint6subCA1Cert.crt \
      $certs/pathLenConstraint6CACert.crt
  delete pathLenConstraint6CACert
  delete pathLenConstraint6subCA1Cert
  delete pathLenConstraint6subsubCA11Cert
  delete pathLenConstraint6subsubsubCA11XCert

  VFY_ACTION="Valid pathLenConstraint Test13"; log_banner
  certImport pathLenConstraint6CACert
  crlImport pathLenConstraint6CACRL.crl
  certImport pathLenConstraint6subCA4Cert
  crlImport pathLenConstraint6subCA4CRL.crl
  certImport pathLenConstraint6subsubCA41Cert
  crlImport pathLenConstraint6subsubCA41CRL.crl
  certImport pathLenConstraint6subsubsubCA41XCert
  crlImport pathLenConstraint6subsubsubCA41XCRL.crl
  pkits $certs/ValidpathLenConstraintTest13EE.crt \
      $certs/pathLenConstraint6subsubsubCA41XCert.crt \
      $certs/pathLenConstraint6subsubCA41Cert.crt \
      $certs/pathLenConstraint6subCA4Cert.crt \
      $certs/pathLenConstraint6CACert.crt
  delete pathLenConstraint6CACert
  delete pathLenConstraint6subCA4Cert
  delete pathLenConstraint6subsubCA41Cert
  delete pathLenConstraint6subsubsubCA41XCert

  VFY_ACTION="Valid pathLenConstraint Test14"; log_banner
  certImport pathLenConstraint6CACert
  crlImport pathLenConstraint6CACRL.crl
  certImport pathLenConstraint6subCA4Cert
  crlImport pathLenConstraint6subCA4CRL.crl
  certImport pathLenConstraint6subsubCA41Cert
  crlImport pathLenConstraint6subsubCA41CRL.crl
  certImport pathLenConstraint6subsubsubCA41XCert
  crlImport pathLenConstraint6subsubsubCA41XCRL.crl
  pkits $certs/ValidpathLenConstraintTest14EE.crt \
      $certs/pathLenConstraint6subsubsubCA41XCert.crt \
      $certs/pathLenConstraint6subsubCA41Cert.crt \
      $certs/pathLenConstraint6subCA4Cert.crt \
      $certs/pathLenConstraint6CACert.crt
  delete pathLenConstraint6CACert
  delete pathLenConstraint6subCA4Cert
  delete pathLenConstraint6subsubCA41Cert
  delete pathLenConstraint6subsubsubCA41XCert

  VFY_ACTION="Valid Self-Issued pathLenConstraint Test15"; log_banner
  certImport pathLenConstraint0CACert
  crlImport pathLenConstraint0CACRL.crl
  pkits $certs/ValidSelfIssuedpathLenConstraintTest15EE.crt \
      $certs/pathLenConstraint0SelfIssuedCACert.crt \
      $certs/pathLenConstraint0CACert.crt
  delete pathLenConstraint0CACert

  VFY_ACTION="Invalid Self-Issued pathLenConstraint Test16"; log_banner
  certImport pathLenConstraint0CACert
  crlImport pathLenConstraint0CACRL.crl
  certImport pathLenConstraint0subCA2Cert
  crlImport pathLenConstraint0subCA2CRL.crl
  pkitsn $certs/InvalidSelfIssuedpathLenConstraintTest16EE.crt \
      $certs/pathLenConstraint0subCA2Cert.crt \
      $certs/pathLenConstraint0SelfIssuedCACert.crt \
      $certs/pathLenConstraint0CACert.crt
  delete pathLenConstraint0CACert
  delete pathLenConstraint0subCA2Cert

  VFY_ACTION="Valid Self-Issued pathLenConstraint Test17"; log_banner
  certImport pathLenConstraint1CACert
  crlImport pathLenConstraint1CACRL.crl
  certImport pathLenConstraint1subCACert
  crlImport pathLenConstraint1subCACRL.crl
  pkits $certs/ValidSelfIssuedpathLenConstraintTest17EE.crt \
      $certs/pathLenConstraint1SelfIssuedsubCACert.crt \
      $certs/pathLenConstraint1subCACert.crt \
      $certs/pathLenConstraint1SelfIssuedCACert.crt \
      $certs/pathLenConstraint1CACert.crt
  delete pathLenConstraint1CACert
  delete pathLenConstraint1subCACert
}

pkits_KeyUsage()
{
  break_table "NIST PKITS Section 4.7: Key Usage"

  VFY_ACTION="Invalid keyUsage Critical keyCertSign False Test1"; log_banner
  certImport keyUsageCriticalkeyCertSignFalseCACert
  crlImport keyUsageCriticalkeyCertSignFalseCACRL.crl
  pkitsn $certs/InvalidkeyUsageCriticalkeyCertSignFalseTest1EE.crt \
      $certs/keyUsageCriticalkeyCertSignFalseCACert.crt
  delete keyUsageCriticalkeyCertSignFalseCACert

  VFY_ACTION="Invalid keyUsage Not Critical keyCertSign False Test2"; log_banner
  certImport keyUsageNotCriticalkeyCertSignFalseCACert
  crlImport keyUsageNotCriticalkeyCertSignFalseCACRL.crl
  pkitsn $certs/InvalidkeyUsageNotCriticalkeyCertSignFalseTest2EE.crt \
      $certs/keyUsageNotCriticalkeyCertSignFalseCACert.crt
  delete keyUsageNotCriticalkeyCertSignFalseCACert

  VFY_ACTION="Valid keyUsage Not Critical Test3"; log_banner
  certImport keyUsageNotCriticalCACert
  crlImport keyUsageNotCriticalCACRL.crl
  pkits $certs/ValidkeyUsageNotCriticalTest3EE.crt \
      $certs/keyUsageNotCriticalCACert.crt
  delete keyUsageNotCriticalCACert

  VFY_ACTION="Invalid keyUsage Critical cRLSign False Test4"; log_banner
  certImport keyUsageCriticalcRLSignFalseCACert
  crlImportn keyUsageCriticalcRLSignFalseCACRL.crl
  if [ $RET -eq 0 ] ; then 
      pkitsn $certs/InvalidkeyUsageCriticalcRLSignFalseTest4EE.crt \
	  $certs/keyUsageCriticalcRLSignFalseCACert.crt
  fi
  delete keyUsageCriticalcRLSignFalseCACert

  VFY_ACTION="Invalid keyUsage Not Critical cRLSign False Test5"; log_banner
  certImport keyUsageNotCriticalcRLSignFalseCACert
  crlImportn keyUsageNotCriticalcRLSignFalseCACRL.crl
  if [ $RET -eq 0 ] ; then 
      pkitsn $certs/InvalidkeyUsageNotCriticalcRLSignFalseTest5EE.crt \
	  $certs/keyUsageNotCriticalcRLSignFalseCACert.crt
  fi
  delete keyUsageNotCriticalcRLSignFalseCACert
}

pkits_NameConstraints()
{
  break_table "NIST PKITS Section 4.13: Name Constraints"

  VFY_ACTION="Valid DN nameConstraints Test1"; log_banner
  certImport nameConstraintsDN1CACert
  crlImport nameConstraintsDN1CACRL.crl
  pkits $certs/ValidDNnameConstraintsTest1EE.crt \
      $certs/nameConstraintsDN1CACert.crt
  delete nameConstraintsDN1CACert

  VFY_ACTION="Invalid DN nameConstraints Test2"; log_banner
  certImport nameConstraintsDN1CACert
  crlImport nameConstraintsDN1CACRL.crl
  pkitsn $certs/InvalidDNnameConstraintsTest2EE.crt \
      $certs/nameConstraintsDN1CACert.crt
  delete nameConstraintsDN1CACert

  VFY_ACTION="Invalid DN nameConstraints Test3"; log_banner
  certImport nameConstraintsDN1CACert
  crlImport nameConstraintsDN1CACRL.crl
  pkitsn $certs/InvalidDNnameConstraintsTest3EE.crt \
      $certs/nameConstraintsDN1CACert.crt
  delete nameConstraintsDN1CACert

  VFY_ACTION="Valid DN nameConstraints Test4"; log_banner
  certImport nameConstraintsDN1CACert
  crlImport nameConstraintsDN1CACRL.crl
  pkits $certs/ValidDNnameConstraintsTest4EE.crt \
      $certs/nameConstraintsDN1CACert.crt
  delete nameConstraintsDN1CACert

  VFY_ACTION="Valid DN nameConstraints Test5"; log_banner
  certImport nameConstraintsDN2CACert
  crlImport nameConstraintsDN2CACRL.crl
  pkits $certs/ValidDNnameConstraintsTest5EE.crt \
      $certs/nameConstraintsDN2CACert.crt
  delete nameConstraintsDN2CACert

  VFY_ACTION="Valid DN nameConstraints Test6"; log_banner
  certImport nameConstraintsDN3CACert
  crlImport nameConstraintsDN3CACRL.crl
  pkits $certs/ValidDNnameConstraintsTest6EE.crt \
      $certs/nameConstraintsDN3CACert.crt
  delete nameConstraintsDN3CACert

  VFY_ACTION="Invalid DN nameConstraints Test7"; log_banner
  certImport nameConstraintsDN3CACert
  crlImport nameConstraintsDN3CACRL.crl
  pkitsn $certs/InvalidDNnameConstraintsTest7EE.crt \
      $certs/nameConstraintsDN3CACert.crt
  delete nameConstraintsDN3CACert

  VFY_ACTION="Invalid DN nameConstraints Test8"; log_banner
  certImport nameConstraintsDN4CACert
  crlImport nameConstraintsDN4CACRL.crl
  pkitsn $certs/InvalidDNnameConstraintsTest8EE.crt \
      $certs/nameConstraintsDN4CACert.crt
  delete nameConstraintsDN4CACert

  VFY_ACTION="Invalid DN nameConstraints Test9"; log_banner
  certImport nameConstraintsDN4CACert
  crlImport nameConstraintsDN4CACRL.crl
  pkitsn $certs/InvalidDNnameConstraintsTest9EE.crt \
      $certs/nameConstraintsDN4CACert.crt
  delete nameConstraintsDN4CACert

  VFY_ACTION="Invalid DN nameConstraints Test10"; log_banner
  certImport nameConstraintsDN5CACert
  crlImport nameConstraintsDN5CACRL.crl
  pkitsn $certs/InvalidDNnameConstraintsTest10EE.crt \
      $certs/nameConstraintsDN5CACert.crt
  delete nameConstraintsDN5CACert

  VFY_ACTION="Valid DN nameConstraints Test11"; log_banner
  certImport nameConstraintsDN5CACert
  crlImport nameConstraintsDN5CACRL.crl
  pkits $certs/ValidDNnameConstraintsTest11EE.crt \
      $certs/nameConstraintsDN5CACert.crt
  delete nameConstraintsDN5CACert

  VFY_ACTION="Invalid DN nameConstraints Test12"; log_banner
  certImport nameConstraintsDN1CACert
  crlImport nameConstraintsDN1CACRL.crl
  certImport nameConstraintsDN1subCA1Cert
  crlImport nameConstraintsDN1subCA1CRL.crl
  pkitsn $certs/InvalidDNnameConstraintsTest12EE.crt \
      $certs/nameConstraintsDN1subCA1Cert.crt \
      $certs/nameConstraintsDN1CACert.crt
  delete nameConstraintsDN1CACert
  delete nameConstraintsDN1subCA1Cert

  VFY_ACTION="Invalid DN nameConstraints Test13"; log_banner
  certImport nameConstraintsDN1CACert
  crlImport nameConstraintsDN1CACRL.crl
  certImport nameConstraintsDN1subCA2Cert
  crlImport nameConstraintsDN1subCA2CRL.crl
  pkitsn $certs/InvalidDNnameConstraintsTest13EE.crt \
      $certs/nameConstraintsDN1subCA2Cert.crt \
      $certs/nameConstraintsDN1CACert.crt
  delete nameConstraintsDN1CACert
  delete nameConstraintsDN1subCA2Cert

  VFY_ACTION="Valid DN nameConstraints Test14"; log_banner
  certImport nameConstraintsDN1CACert
  crlImport nameConstraintsDN1CACRL.crl
  certImport nameConstraintsDN1subCA2Cert
  crlImport nameConstraintsDN1subCA2CRL.crl
  pkits $certs/ValidDNnameConstraintsTest14EE.crt \
      $certs/nameConstraintsDN1subCA2Cert.crt \
      $certs/nameConstraintsDN1CACert.crt
  delete nameConstraintsDN1CACert
  delete nameConstraintsDN1subCA2Cert

  VFY_ACTION="Invalid DN nameConstraints Test15"; log_banner
  certImport nameConstraintsDN3CACert
  crlImport nameConstraintsDN3CACRL.crl
  certImport nameConstraintsDN3subCA1Cert
  crlImport nameConstraintsDN3subCA1CRL.crl
  pkitsn $certs/InvalidDNnameConstraintsTest15EE.crt \
      $certs/nameConstraintsDN3subCA1Cert.crt \
      $certs/nameConstraintsDN3CACert.crt
  delete nameConstraintsDN3subCA1Cert
  delete nameConstraintsDN3CACert

  VFY_ACTION="Invalid DN nameConstraints Test16"; log_banner
  certImport nameConstraintsDN3CACert
  crlImport nameConstraintsDN3CACRL.crl
  certImport nameConstraintsDN3subCA1Cert
  crlImport nameConstraintsDN3subCA1CRL.crl
  pkitsn $certs/InvalidDNnameConstraintsTest16EE.crt \
      $certs/nameConstraintsDN3subCA1Cert.crt \
      $certs/nameConstraintsDN3CACert.crt
  delete nameConstraintsDN3subCA1Cert
  delete nameConstraintsDN3CACert

  VFY_ACTION="Invalid DN nameConstraints Test17"; log_banner
  certImport nameConstraintsDN3CACert
  crlImport nameConstraintsDN3CACRL.crl
  certImport nameConstraintsDN3subCA2Cert
  crlImport nameConstraintsDN3subCA2CRL.crl
  pkitsn $certs/InvalidDNnameConstraintsTest17EE.crt \
      $certs/nameConstraintsDN3subCA2Cert.crt \
      $certs/nameConstraintsDN3CACert.crt
  delete nameConstraintsDN3subCA2Cert
  delete nameConstraintsDN3CACert

  VFY_ACTION="Valid DN nameConstraints Test18"; log_banner
  certImport nameConstraintsDN3CACert
  crlImport nameConstraintsDN3CACRL.crl
  certImport nameConstraintsDN3subCA2Cert
  crlImport nameConstraintsDN3subCA2CRL.crl
  pkits $certs/ValidDNnameConstraintsTest18EE.crt \
      $certs/nameConstraintsDN3subCA2Cert.crt \
      $certs/nameConstraintsDN3CACert.crt
  delete nameConstraintsDN3subCA2Cert
  delete nameConstraintsDN3CACert

  VFY_ACTION="Valid Self-Issued DN nameConstraints Test19"; log_banner
  certImport nameConstraintsDN1CACert
  crlImport nameConstraintsDN1CACRL.crl
  pkits $certs/ValidDNnameConstraintsTest19EE.crt \
      $certs/nameConstraintsDN1SelfIssuedCACert.crt \
      $certs/nameConstraintsDN1CACert.crt
  delete nameConstraintsDN1CACert

  VFY_ACTION="Invalid Self-Issued DN nameConstraints Test20"; log_banner
  certImport nameConstraintsDN1CACert
  crlImport nameConstraintsDN1CACRL.crl
  pkitsn $certs/InvalidDNnameConstraintsTest20EE.crt \
      $certs/nameConstraintsDN1CACert.crt
  delete nameConstraintsDN1CACert

  VFY_ACTION="Valid RFC822 nameConstraints Test21"; log_banner
  certImport nameConstraintsRFC822CA1Cert
  crlImport nameConstraintsRFC822CA1CRL.crl
  pkits $certs/ValidRFC822nameConstraintsTest21EE.crt \
      $certs/nameConstraintsRFC822CA1Cert.crt
  delete nameConstraintsRFC822CA1Cert

  VFY_ACTION="Invalid RFC822 nameConstraints Test22"; log_banner
  certImport nameConstraintsRFC822CA1Cert
  crlImport nameConstraintsRFC822CA1CRL.crl
  pkitsn $certs/InvalidRFC822nameConstraintsTest22EE.crt \
      $certs/nameConstraintsRFC822CA1Cert.crt
  delete nameConstraintsRFC822CA1Cert

  VFY_ACTION="Valid RFC822 nameConstraints Test23"; log_banner
  certImport nameConstraintsRFC822CA2Cert
  crlImport nameConstraintsRFC822CA2CRL.crl
  pkits $certs/ValidRFC822nameConstraintsTest23EE.crt \
      $certs/nameConstraintsRFC822CA2Cert.crt
  delete nameConstraintsRFC822CA2Cert

  VFY_ACTION="Invalid RFC822 nameConstraints Test24"; log_banner
  certImport nameConstraintsRFC822CA2Cert
  crlImport nameConstraintsRFC822CA2CRL.crl
  pkitsn $certs/InvalidRFC822nameConstraintsTest24EE.crt \
      $certs/nameConstraintsRFC822CA2Cert.crt
  delete nameConstraintsRFC822CA2Cert

  VFY_ACTION="Valid RFC822 nameConstraints Test25"; log_banner
  certImport nameConstraintsRFC822CA3Cert
  crlImport nameConstraintsRFC822CA3CRL.crl
  pkits $certs/ValidRFC822nameConstraintsTest25EE.crt \
      $certs/nameConstraintsRFC822CA3Cert.crt
  delete nameConstraintsRFC822CA3Cert

  VFY_ACTION="Invalid RFC822 nameConstraints Test26"; log_banner
  certImport nameConstraintsRFC822CA3Cert
  crlImport nameConstraintsRFC822CA3CRL.crl
  pkitsn $certs/InvalidRFC822nameConstraintsTest26EE.crt \
      $certs/nameConstraintsRFC822CA3Cert.crt
  delete nameConstraintsRFC822CA3Cert

  VFY_ACTION="Valid DN and RFC822 nameConstraints Test27"; log_banner
  certImport nameConstraintsDN1CACert
  crlImport nameConstraintsDN1CACRL.crl
  certImport nameConstraintsDN1subCA3Cert
  crlImport nameConstraintsDN1subCA3CRL.crl
  pkits $certs/ValidDNandRFC822nameConstraintsTest27EE.crt \
      $certs/nameConstraintsDN1subCA3Cert.crt \
      $certs/nameConstraintsDN1CACert.crt
  delete nameConstraintsDN1CACert
  delete nameConstraintsDN1subCA3Cert

  VFY_ACTION="Invalid DN and RFC822 nameConstraints Test28"; log_banner
  certImport nameConstraintsDN1CACert
  crlImport nameConstraintsDN1CACRL.crl
  certImport nameConstraintsDN1subCA3Cert
  crlImport nameConstraintsDN1subCA3CRL.crl
  pkitsn $certs/InvalidDNandRFC822nameConstraintsTest28EE.crt \
      $certs/nameConstraintsDN1subCA3Cert.crt \
      $certs/nameConstraintsDN1CACert.crt
  delete nameConstraintsDN1CACert
  delete nameConstraintsDN1subCA3Cert

  VFY_ACTION="Invalid DN and RFC822 nameConstraints Test29"; log_banner
  certImport nameConstraintsDN1CACert
  crlImport nameConstraintsDN1CACRL.crl
  certImport nameConstraintsDN1subCA3Cert
  crlImport nameConstraintsDN1subCA3CRL.crl
  pkitsn $certs/InvalidDNandRFC822nameConstraintsTest29EE.crt \
      $certs/nameConstraintsDN1subCA3Cert.crt \
      $certs/nameConstraintsDN1CACert.crt
  delete nameConstraintsDN1CACert
  delete nameConstraintsDN1subCA3Cert

  VFY_ACTION="Valid DNS nameConstraints Test30"; log_banner
  certImport nameConstraintsDNS1CACert
  crlImport nameConstraintsDNS1CACRL.crl
  pkits $certs/ValidDNSnameConstraintsTest30EE.crt \
      $certs/nameConstraintsDNS1CACert.crt
  delete nameConstraintsDNS1CACert

  VFY_ACTION="Invalid DNS nameConstraints Test31"; log_banner
  certImport nameConstraintsDNS1CACert
  crlImport nameConstraintsDNS1CACRL.crl
  pkitsn $certs/InvalidDNSnameConstraintsTest31EE.crt \
      $certs/nameConstraintsDNS1CACert.crt
  delete nameConstraintsDNS1CACert

  VFY_ACTION="Valid DNS nameConstraints Test32"; log_banner
  certImport nameConstraintsDNS2CACert
  crlImport nameConstraintsDNS2CACRL.crl
  pkits $certs/ValidDNSnameConstraintsTest32EE.crt \
      $certs/nameConstraintsDNS2CACert.crt
  delete nameConstraintsDNS2CACert

  VFY_ACTION="Invalid DNS nameConstraints Test33"; log_banner
  certImport nameConstraintsDNS2CACert
  crlImport nameConstraintsDNS2CACRL.crl
  pkitsn $certs/InvalidDNSnameConstraintsTest33EE.crt \
      $certs/nameConstraintsDNS2CACert.crt
  delete nameConstraintsDNS2CACert

  VFY_ACTION="Valid URI nameConstraints Test34"; log_banner
  certImport nameConstraintsURI1CACert
  crlImport nameConstraintsURI1CACRL.crl
  pkits $certs/ValidURInameConstraintsTest34EE.crt \
      $certs/nameConstraintsURI1CACert.crt
  delete nameConstraintsURI1CACert

  VFY_ACTION="Invalid URI nameConstraints Test35"; log_banner
  certImport nameConstraintsURI1CACert
  crlImport nameConstraintsURI1CACRL.crl
  pkitsn $certs/InvalidURInameConstraintsTest35EE.crt \
      $certs/nameConstraintsURI1CACert.crt
  delete nameConstraintsURI1CACert

  VFY_ACTION="Valid URI nameConstraints Test36"; log_banner
  certImport nameConstraintsURI2CACert
  crlImport nameConstraintsURI2CACRL.crl
  pkits $certs/ValidURInameConstraintsTest36EE.crt \
      $certs/nameConstraintsURI2CACert.crt
  delete nameConstraintsURI2CACert

  VFY_ACTION="Invalid URI nameConstraints Test37"; log_banner
  certImport nameConstraintsURI2CACert
  crlImport nameConstraintsURI2CACRL.crl
  pkitsn $certs/InvalidURInameConstraintsTest37EE.crt \
      $certs/nameConstraintsURI2CACert.crt
  delete nameConstraintsURI2CACert

  VFY_ACTION="Invalid DNS nameConstraints Test38"; log_banner
  certImport nameConstraintsDNS1CACert
  crlImport nameConstraintsDNS1CACRL.crl
  pkitsn $certs/InvalidDNSnameConstraintsTest38EE.crt \
      $certs/nameConstraintsDNS1CACert.crt
  delete nameConstraintsDNS1CACert
}

pkits_PvtCertExtensions()
{
  break_table "NIST PKITS Section 4.16: Private Certificate Extensions"

  VFY_ACTION="Valid Unknown Not Critical Certificate Extension Test1"; log_banner
  pkits $certs/ValidUnknownNotCriticalCertificateExtensionTest1EE.crt

  VFY_ACTION="Invalid Unknown Critical Certificate Extension Test2"; log_banner
  pkitsn $certs/InvalidUnknownCriticalCertificateExtensionTest2EE.crt
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
pkits_SignatureVerification | tee -a $PKITS_LOG
pkits_ValidityPeriods | tee -a $PKITS_LOG
pkits_NameChaining | tee -a $PKITS_LOG
pkits_BasicCertRevocation | tee -a $PKITS_LOG
pkits_PathVerificWithSelfIssuedCerts | tee -a $PKITS_LOG
pkits_BasicConstraints | tee -a $PKITS_LOG
pkits_KeyUsage | tee -a $PKITS_LOG
pkits_NameConstraints | tee -a $PKITS_LOG
pkits_PvtCertExtensions | tee -a $PKITS_LOG
pkits_cleanup
