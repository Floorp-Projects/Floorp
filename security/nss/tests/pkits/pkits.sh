#!/bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

  if [ -z "${PKITS_DATA}" ]; then
      echo "${SCRIPTNAME}: PKITS data directory not defined, skipping."
      exit 0
  fi      

  if [ ! -d "${PKITS_DATA}" ]; then
      echo "${SCRIPTNAME}: PKITS data directory ${PKITS_DATA} doesn't exist, skipping."
      exit 0
  fi

  PKITSDIR=${HOSTDIR}/pkits

  COPYDIR=${PKITSDIR}/copydir

  mkdir -p ${PKITSDIR}
  mkdir -p ${COPYDIR}
  mkdir -p ${PKITSDIR}/html

  certs=${PKITS_DATA}/certs
  crls=${PKITS_DATA}/crls

  cd ${PKITSDIR}

  PKITSdb=${PKITSDIR}/PKITSdb
  PKITSbkp=${PKITSDIR}/PKITSbkp

  PKITS_LOG=${PKITSDIR}/pkits.log #getting its own logfile
  pkits_log "Start of logfile $PKITS_LOG"

  if [ ! -d "${PKITSdb}" ]; then
      mkdir -p ${PKITSdb}
  else
      pkits_log "$SCRIPTNAME: WARNING - ${PKITSdb} exists"
  fi

  if [ ! -d "${PKITSbkp}" ]; then
      mkdir -p ${PKITSbkp}
  else
      pkits_log "$SCRIPTNAME: WARNING - ${PKITSbkp} exists"
  fi

  echo "HOSTDIR" $HOSTDIR
  echo "PKITSDIR" $PKITSDIR
  echo "PKITSdb" $PKITSdb
  echo "PKITSbkp" $PKITSbkp
  echo "PKITS_DATA" $PKITS_DATA
  echo "certs" $certs
  echo "crls" $crls

  echo nss > ${PKITSdb}/pw
  ${BINDIR}/certutil -N -d ${PKITSdb} -f ${PKITSdb}/pw

  ${BINDIR}/certutil -A -n TrustAnchorRootCertificate -t "C,C,C" -i \
      $certs/TrustAnchorRootCertificate.crt -d $PKITSdb
  if [ -z "$NSS_NO_PKITS_CRLS" ]; then
    ${BINDIR}/crlutil -I -i $crls/TrustAnchorRootCRL.crl -d ${PKITSdb} -f ${PKITSdb}/pw
  else
    html  "<H3>NO CRLs are being used.</H3>"
    pkits_log "NO CRLs are being used."
  fi

  cp ${PKITSdb}/* ${PKITSbkp}

  KNOWN_BUG=
}

############################### pkits_log ##############################
# write to pkits.log file
########################################################################
pkits_log()
{
  echo "$SCRIPTNAME $*"
  echo $* >> ${PKITS_LOG}
}

restore_db()
{
  echo "Restore DB"
  rm ${PKITSdb}/*
  cp ${PKITSbkp}/* ${PKITSdb}
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
  ${BINDIR}/vfychain -d $PKITSdb -u 4 $* > ${PKITSDIR}/cmdout.txt 2>&1
  RET=$?
  CNT=`grep -c ERROR ${PKITSDIR}/cmdout.txt`
  RET=`expr ${RET} + ${CNT}`
  cat ${PKITSDIR}/cmdout.txt

  if [ "$RET" -ne 0 ]; then
      html_failed "${VFY_ACTION} ($RET) "
      pkits_log "ERROR: ${VFY_ACTION} failed $RET"
  else
      html_passed "${VFY_ACTION}"
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
  ${BINDIR}/vfychain -d $PKITSdb -u 4 $* > ${PKITSDIR}/cmdout.txt 2>&1
  RET=$?
  CNT=`grep -c ERROR ${PKITSDIR}/cmdout.txt`
  RET=`expr ${RET} + ${CNT}`
  cat ${PKITSDIR}/cmdout.txt

  if [ "$RET" -eq 0 ]; then
      html_failed "${VFY_ACTION} ($RET) "
      pkits_log "ERROR: ${VFY_ACTION} failed $RET"
  else
      html_passed "${VFY_ACTION} ($RET) "
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
  if [ -z "$NSS_NO_PKITS_CRLS" ]; then
    echo "crlutil -d $PKITSdb -I -f ${PKITSdb}/pw -i $crls/$*"
    ${BINDIR}/crlutil -d ${PKITSdb} -I -f ${PKITSdb}/pw -i $crls/$* > ${PKITSDIR}/cmdout.txt 2>&1
    RET=$?
    cat ${PKITSDIR}/cmdout.txt

    if [ "$RET" -ne 0 ]; then
        html_failed "${VFY_ACTION} ($RET) "
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
  if [ -z "$NSS_NO_PKITS_CRLS" ]; then
    echo "crlutil -d $PKITSdb -I -f ${PKITSdb}/pw -i $crls/$*"
    ${BINDIR}/crlutil -d ${PKITSdb} -I -f ${PKITSdb}/pw -i $crls/$* > ${PKITSDIR}/cmdout.txt 2>&1
    RET=$?
    cat ${PKITSDIR}/cmdout.txt

    if [ "$RET" -eq 0 ]; then
        html_failed "${VFY_ACTION} ($RET) "
        pkits_log "ERROR: ${VFY_ACTION} failed $RET"
    else
        html_passed "${VFY_ACTION} ($RET) "
        pkits_log "SUCCESS: ${VFY_ACTION} returned as expected $RET"
    fi
  fi
  return $RET
}

################################ certImport #############################
# local shell function to import a Cert, calls certutil -A, writes 
# action and options to stdout
########################################################################
certImport()
{
  echo "certutil -d $PKITSdb -A -t \",,\" -n $* -i $certs/$*.crt"
  ${BINDIR}/certutil -d $PKITSdb -A -t ",," -n $* -i $certs/$*.crt > ${PKITSDIR}/cmdout.txt 2>&1
  RET=$?
  cat ${PKITSDIR}/cmdout.txt

  if [ "$RET" -ne 0 ]; then
      html_failed "${VFY_ACTION} ($RET) "
      pkits_log "ERROR: ${VFY_ACTION} failed $RET"
  fi
}

################################ certImportn #############################
# local shell function to import an incorrect Cert, calls certutil -A, 
# writes action and options to stdout
########################################################################
certImportn()
{
  RET=0
  if [ -z "$NSS_NO_PKITS_CRLS" ]; then
    echo "certutil -d $PKITSdb -A -t \",,\" -n $* -i $certs/$*.crt"
    ${BINDIR}/certutil -d $PKITSdb -A -t ",," -n $* -i $certs/$*.crt > ${PKITSDIR}/cmdout.txt 2>&1
    RET=$?
    cat ${PKITSDIR}/cmdout.txt

    if [ "$RET" -eq 0 ]; then
        html_failed "${VFY_ACTION} ($RET) "
        pkits_log "ERROR: ${VFY_ACTION} failed $RET"
    else
        html_passed "${VFY_ACTION} ($RET) "
        pkits_log "SUCCESS: ${VFY_ACTION} returned as expected $RET"
    fi
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
  restore_db

  VFY_ACTION="Invalid CA Signature Test2"; log_banner
  certImport BadSignedCACert
  crlImport BadSignedCACRL.crl
  pkitsn $certs/InvalidCASignatureTest2EE.crt \
    $certs/BadSignedCACert.crt
  restore_db

  VFY_ACTION="Invalid EE Signature Test3"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkitsn $certs/InvalidEESignatureTest3EE.crt $certs/GoodCACert.crt
  restore_db

  VFY_ACTION="Valid DSA Signatures Test4"; log_banner
  certImport DSACACert
  crlImport DSACACRL.crl
  pkits $certs/ValidDSASignaturesTest4EE.crt $certs/DSACACert.crt
  restore_db

  VFY_ACTION="Valid DSA Parameter Inheritance Test5"; log_banner
  certImport DSACACert
  crlImport DSACACRL.crl
  certImport DSAParametersInheritedCACert
  crlImport DSAParametersInheritedCACRL.crl
  pkits $certs/ValidDSAParameterInheritanceTest5EE.crt \
      $certs/DSAParametersInheritedCACert.crt \
      $certs/DSACACert.crt
  restore_db

  VFY_ACTION="Invalid DSA Signature Test6"; log_banner
  certImport DSACACert
  crlImport DSACACRL.crl
  pkitsn $certs/InvalidDSASignatureTest6EE.crt $certs/DSACACert.crt
  restore_db
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
  restore_db

  VFY_ACTION="Invalid EE notBefore Date Test2"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkitsn $certs/InvalidEEnotBeforeDateTest2EE.crt \
      $certs/GoodCACert.crt
  restore_db

  VFY_ACTION="Valid pre2000 UTC notBefore Date Test3"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkits $certs/Validpre2000UTCnotBeforeDateTest3EE.crt \
      $certs/GoodCACert.crt
  restore_db

  VFY_ACTION="Valid GeneralizedTime notBefore Date Test4"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkits $certs/ValidGeneralizedTimenotBeforeDateTest4EE.crt \
      $certs/GoodCACert.crt
  restore_db

  VFY_ACTION="Invalid CA notAfter Date Test5"; log_banner
  certImport BadnotAfterDateCACert
  crlImportn BadnotAfterDateCACRL.crl
  if [ $RET -eq 0 ] ; then 
      pkitsn $certs/InvalidCAnotAfterDateTest5EE.crt \
          $certs/BadnotAfterDateCACert.crt
  fi
  restore_db

  VFY_ACTION="Invalid EE notAfter Date Test6"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkitsn $certs/InvalidEEnotAfterDateTest6EE.crt \
      $certs/GoodCACert.crt
  restore_db

  VFY_ACTION="Invalid pre2000 UTC EE notAfter Date Test7"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkitsn $certs/Invalidpre2000UTCEEnotAfterDateTest7EE.crt \
      $certs/GoodCACert.crt
  restore_db

  VFY_ACTION="ValidGeneralizedTime notAfter Date Test8"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkits $certs/ValidGeneralizedTimenotAfterDateTest8EE.crt \
      $certs/GoodCACert.crt
  restore_db
}

pkits_NameChaining()
{
  break_table "NIST PKITS Section 4.3: Verifying NameChaining"

  VFY_ACTION="Invalid Name Chaining EE Test1"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkitsn $certs/InvalidNameChainingTest1EE.crt \
      $certs/GoodCACert.crt
  restore_db

  VFY_ACTION="Invalid Name Chaining Order Test2"; log_banner
  certImport NameOrderingCACert
  crlImport NameOrderCACRL.crl
  pkitsn $certs/InvalidNameChainingOrderTest2EE.crt \
      $certs/NameOrderingCACert.crt
  restore_db

### bug 216123 ###
if [ -n "${KNOWN_BUG}" ]; then
  VFY_ACTION="Valid Name Chaining Whitespace Test3"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkits $certs/ValidNameChainingWhitespaceTest3EE.crt \
      $certs/GoodCACert.crt
  restore_db

  VFY_ACTION="Valid Name Chaining Whitespace Test4"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkits $certs/ValidNameChainingWhitespaceTest4EE.crt \
      $certs/GoodCACert.crt
  restore_db

  VFY_ACTION="Valid Name Chaining Capitalization Test5"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkits $certs/ValidNameChainingCapitalizationTest5EE.crt \
      $certs/GoodCACert.crt
  restore_db
fi

  VFY_ACTION="Valid Name Chaining UIDs Test6"; log_banner
  certImport UIDCACert
  crlImport UIDCACRL.crl
  pkits $certs/ValidNameUIDsTest6EE.crt $certs/UIDCACert.crt
  restore_db

  VFY_ACTION="Valid RFC3280 Mandatory Attribute Types Test7"; log_banner
  certImport RFC3280MandatoryAttributeTypesCACert
  crlImport RFC3280MandatoryAttributeTypesCACRL.crl
  pkits $certs/ValidRFC3280MandatoryAttributeTypesTest7EE.crt \
      $certs/RFC3280MandatoryAttributeTypesCACert.crt
  restore_db

  VFY_ACTION="Valid RFC3280 Optional Attribute Types Test8"; log_banner
  certImport RFC3280OptionalAttributeTypesCACert
  crlImport RFC3280OptionalAttributeTypesCACRL.crl
  pkits $certs/ValidRFC3280OptionalAttributeTypesTest8EE.crt \
      $certs/RFC3280OptionalAttributeTypesCACert.crt
  restore_db

  VFY_ACTION="Valid UTF8String Encoded Names Test9"; log_banner
  certImport UTF8StringEncodedNamesCACert
  crlImport UTF8StringEncodedNamesCACRL.crl
  pkits $certs/ValidUTF8StringEncodedNamesTest9EE.crt \
      $certs/UTF8StringEncodedNamesCACert.crt
  restore_db

### bug 216123 ###
if [ -n "${KNOWN_BUG}" ]; then
  VFY_ACTION="Valid Rollover from PrintableString to UTF8String Test10"; log_banner
  certImport RolloverfromPrintableStringtoUTF8StringCACert
  crlImport RolloverfromPrintableStringtoUTF8StringCACRL.crl
  pkits $certs/ValidRolloverfromPrintableStringtoUTF8StringTest10EE.crt \
      $certs/RolloverfromPrintableStringtoUTF8StringCACert.crt
  restore_db

  VFY_ACTION="Valid UTF8String case Insensitive Match Test11"; log_banner
  certImport UTF8StringCaseInsensitiveMatchCACert
  crlImport UTF8StringCaseInsensitiveMatchCACRL.crl
  pkits $certs/ValidUTF8StringCaseInsensitiveMatchTest11EE.crt \
      $certs/UTF8StringCaseInsensitiveMatchCACert.crt
  restore_db
fi
}

pkits_BasicCertRevocation()
{
  break_table "NIST PKITS Section 4.4: Basic Certificate Revocation Tests"

### bug 414556 ###
if [ -n "${KNOWN_BUG}" ]; then
  VFY_ACTION="Missing CRL Test1"; log_banner
  pkitsn $certs/InvalidMissingCRLTest1EE.crt \
      $certs/NoCRLCACert.crt
fi

  VFY_ACTION="Invalid Revoked CA Test2"; log_banner
  certImport RevokedsubCACert
  crlImport RevokedsubCACRL.crl
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkitsn $certs/InvalidRevokedCATest2EE.crt \
     $certs/RevokedsubCACert.crt $certs/GoodCACert.crt
  restore_db

  VFY_ACTION="Invalid Revoked EE Test3"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkitsn $certs/InvalidRevokedEETest3EE.crt \
     $certs/GoodCACert.crt
  restore_db

  VFY_ACTION="Invalid Bad CRL Signature Test4"; log_banner
  certImport BadCRLSignatureCACert
  crlImportn BadCRLSignatureCACRL.crl
  if [ $RET -eq 0 ] ; then 
      pkitsn $certs/InvalidBadCRLSignatureTest4EE.crt \
          $certs/BadCRLSignatureCACert.crt
  fi
  restore_db

  VFY_ACTION="Invalid Bad CRL Issuer Name Test5"; log_banner
  certImport BadCRLIssuerNameCACert
  crlImportn BadCRLIssuerNameCACRL.crl
  if [ $RET -eq 0 ] ; then 
      pkitsn $certs/InvalidBadCRLIssuerNameTest5EE.crt \
          $certs/BadCRLIssuerNameCACert.crt
  fi
  restore_db

### bug 414556 ###
if [ -n "${KNOWN_BUG}" ]; then
  VFY_ACTION="Invalid Wrong CRL Test6"; log_banner
  certImport WrongCRLCACert
  crlImport WrongCRLCACRL.crl
  pkitsn $certs/InvalidWrongCRLTest6EE.crt \
      $certs/WrongCRLCACert.crt
  restore_db
fi

  VFY_ACTION="Valid Two CRLs Test7"; log_banner
  certImport TwoCRLsCACert
  crlImport TwoCRLsCAGoodCRL.crl
  crlImportn TwoCRLsCABadCRL.crl
  pkits $certs/ValidTwoCRLsTest7EE.crt \
     $certs/TwoCRLsCACert.crt
  restore_db

  VFY_ACTION="Invalid Unknown CRL Entry Extension Test8"; log_banner
  certImport UnknownCRLEntryExtensionCACert
  crlImportn UnknownCRLEntryExtensionCACRL.crl
  if [ $RET -eq 0 ] ; then 
      pkitsn $certs/InvalidUnknownCRLEntryExtensionTest8EE.crt \
          $certs/UnknownCRLEntryExtensionCACert.crt
  fi
  restore_db

  VFY_ACTION="Invalid Unknown CRL Extension Test9"; log_banner
  certImport UnknownCRLExtensionCACert
  crlImportn UnknownCRLExtensionCACRL.crl
  if [ $RET -eq 0 ] ; then 
      pkitsn $certs/InvalidUnknownCRLExtensionTest9EE.crt \
          $certs/UnknownCRLExtensionCACert.crt
  fi
  restore_db

  VFY_ACTION="Invalid Unknown CRL Extension Test10"; log_banner
  certImport UnknownCRLExtensionCACert
  crlImportn UnknownCRLExtensionCACRL.crl
  if [ $RET -eq 0 ] ; then 
      pkitsn $certs/InvalidUnknownCRLExtensionTest10EE.crt \
          $certs/UnknownCRLExtensionCACert.crt
  fi
  restore_db

### bug 414563 ###
if [ -n "${KNOWN_BUG}" ]; then
  VFY_ACTION="Invalid Old CRL nextUpdate Test11"; log_banner
  certImport OldCRLnextUpdateCACert
  crlImport OldCRLnextUpdateCACRL.crl
  pkitsn $certs/InvalidOldCRLnextUpdateTest11EE.crt \
     $certs/OldCRLnextUpdateCACert.crt
  restore_db

  VFY_ACTION="Invalid pre2000 CRL nextUpdate Test12"; log_banner
  certImport pre2000CRLnextUpdateCACert
  crlImport pre2000CRLnextUpdateCACRL.crl
  pkitsn $certs/Invalidpre2000CRLnextUpdateTest12EE.crt \
     $certs/pre2000CRLnextUpdateCACert.crt
  restore_db
fi

  VFY_ACTION="Valid GeneralizedTime CRL nextUpdate Test13"; log_banner
  certImport GeneralizedTimeCRLnextUpdateCACert
  crlImport GeneralizedTimeCRLnextUpdateCACRL.crl
  pkits $certs/ValidGeneralizedTimeCRLnextUpdateTest13EE.crt \
     $certs/GeneralizedTimeCRLnextUpdateCACert.crt
  restore_db

  VFY_ACTION="Valid Negative Serial Number Test14"; log_banner
  certImport NegativeSerialNumberCACert
  crlImport NegativeSerialNumberCACRL.crl
  pkits $certs/ValidNegativeSerialNumberTest14EE.crt \
     $certs/NegativeSerialNumberCACert.crt
  restore_db

  VFY_ACTION="Invalid Negative Serial Number Test15"; log_banner
  certImport NegativeSerialNumberCACert
  crlImport NegativeSerialNumberCACRL.crl
  pkitsn $certs/InvalidNegativeSerialNumberTest15EE.crt \
     $certs/NegativeSerialNumberCACert.crt
  restore_db

  VFY_ACTION="Valid Long Serial Number Test16"; log_banner
  certImport LongSerialNumberCACert
  crlImport LongSerialNumberCACRL.crl
  pkits $certs/ValidLongSerialNumberTest16EE.crt \
     $certs/LongSerialNumberCACert.crt
  restore_db

  VFY_ACTION="Valid Long Serial Number Test17"; log_banner
  certImport LongSerialNumberCACert
  crlImport LongSerialNumberCACRL.crl
  pkits $certs/ValidLongSerialNumberTest17EE.crt \
     $certs/LongSerialNumberCACert.crt
  restore_db

  VFY_ACTION="Invalid Long Serial Number Test18"; log_banner
  certImport LongSerialNumberCACert
  crlImport LongSerialNumberCACRL.crl
  pkitsn $certs/InvalidLongSerialNumberTest18EE.crt \
     $certs/LongSerialNumberCACert.crt
  restore_db

### bug 232737 ###
if [ -n "${KNOWN_BUG}" ]; then
  VFY_ACTION="Valid Separate Certificate and CRL Keys Test19"; log_banner
  certImport SeparateCertificateandCRLKeysCertificateSigningCACert
  certImport SeparateCertificateandCRLKeysCRLSigningCert
  crlImport SeparateCertificateandCRLKeysCRL.crl
  pkits $certs/ValidSeparateCertificateandCRLKeysTest19EE.crt \
     $certs/SeparateCertificateandCRLKeysCRLSigningCert.crt
  restore_db

  VFY_ACTION="Invalid Separate Certificate and CRL Keys Test20"; log_banner
  certImport SeparateCertificateandCRLKeysCertificateSigningCACert
  certImport SeparateCertificateandCRLKeysCRLSigningCert
  crlImport SeparateCertificateandCRLKeysCRL.crl
  pkits $certs/InvalidSeparateCertificateandCRLKeysTest20EE.crt \
     $certs/SeparateCertificateandCRLKeysCRLSigningCert.crt
  restore_db

  VFY_ACTION="Invalid Separate Certificate and CRL Keys Test21"; log_banner
  certImport SeparateCertificateandCRLKeysCA2CertificateSigningCACert
  certImport SeparateCertificateandCRLKeysCA2CRLSigningCert
  crlImport SeparateCertificateandCRLKeysCA2CRL.crl
  pkits $certs/InvalidSeparateCertificateandCRLKeysTest21EE.crt \
     $certs/SeparateCertificateandCRLKeysCA2CRLSigningCert.crt
  restore_db
fi
}

pkits_PathVerificWithSelfIssuedCerts()
{
  break_table "NIST PKITS Section 4.5: Self-Issued Certificates"

### bug 232737 ###
if [ -n "${KNOWN_BUG}" ]; then
  VFY_ACTION="Valid Basic Self-Issued Old With New Test1"; log_banner
  certImport BasicSelfIssuedNewKeyCACert
  crlImport BasicSelfIssuedNewKeyCACRL.crl
  pkits $certs/ValidBasicSelfIssuedOldWithNewTest1EE.crt \
      $certs/BasicSelfIssuedNewKeyOldWithNewCACert.crt \
      $certs/BasicSelfIssuedNewKeyCACert.crt
  restore_db

  VFY_ACTION="Invalid Basic Self-Issued Old With New Test2"; log_banner
  certImport BasicSelfIssuedNewKeyCACert
  crlImport BasicSelfIssuedNewKeyCACRL.crl
  pkitsn $certs/InvalidBasicSelfIssuedOldWithNewTest2EE.crt \
      $certs/BasicSelfIssuedNewKeyOldWithNewCACert.crt \
      $certs/BasicSelfIssuedNewKeyCACert.crt
  restore_db
fi

### bugs 321755 & 418769 ###
if [ -n "${KNOWN_BUG}" ]; then
  VFY_ACTION="Valid Basic Self-Issued New With Old Test3"; log_banner
  certImport BasicSelfIssuedOldKeyCACert
  crlImport BasicSelfIssuedOldKeyCACRL.crl
  pkits $certs/ValidBasicSelfIssuedNewWithOldTest3EE.crt \
      $certs/BasicSelfIssuedOldKeyNewWithOldCACert.crt \
      $certs/BasicSelfIssuedOldKeyCACert.crt
  restore_db

  VFY_ACTION="Valid Basic Self-Issued New With Old Test4"; log_banner
  certImport BasicSelfIssuedOldKeyCACert
  crlImport BasicSelfIssuedOldKeyCACRL.crl
  pkits $certs/ValidBasicSelfIssuedNewWithOldTest4EE.crt \
      $certs/BasicSelfIssuedOldKeyNewWithOldCACert.crt \
      $certs/BasicSelfIssuedOldKeyCACert.crt
  restore_db

  VFY_ACTION="Invalid Basic Self-Issued New With Old Test5"; log_banner
  certImport BasicSelfIssuedOldKeyCACert
  crlImport BasicSelfIssuedOldKeyCACRL.crl
  pkitsn $certs/InvalidBasicSelfIssuedNewWithOldTest5EE.crt \
      $certs/BasicSelfIssuedOldKeyNewWithOldCACert.crt \
      $certs/BasicSelfIssuedOldKeyCACert.crt
  restore_db

  VFY_ACTION="Valid Basic Self-Issued CRL Signing Key Test6"; log_banner
  certImport BasicSelfIssuedCRLSigningKeyCACert
  crlImport BasicSelfIssuedOldKeyCACRL.crl
  pkits $certs/ValidBasicSelfIssuedCRLSigningKeyTest6EE.crt \
      $certs/BasicSelfIssuedCRLSigningKeyCRLCert.crt \
      $certs/BasicSelfIssuedCRLSigningKeyCACert.crt
  restore_db

  VFY_ACTION="Invalid Basic Self-Issued CRL Signing Key Test7"; log_banner
  certImport BasicSelfIssuedCRLSigningKeyCACert
  crlImport BasicSelfIssuedOldKeyCACRL.crl
  pkitsn $certs/InvalidBasicSelfIssuedCRLSigningKeyTest7EE.crt \
      $certs/BasicSelfIssuedCRLSigningKeyCRLCert.crt \
      $certs/BasicSelfIssuedCRLSigningKeyCACert.crt
  restore_db

  VFY_ACTION="Invalid Basic Self-Issued CRL Signing Key Test8"; log_banner
  certImport BasicSelfIssuedCRLSigningKeyCACert
  crlImport BasicSelfIssuedOldKeyCACRL.crl
  pkitsn $certs/InvalidBasicSelfIssuedCRLSigningKeyTest8EE.crt \
      $certs/BasicSelfIssuedCRLSigningKeyCRLCert.crt \
      $certs/BasicSelfIssuedCRLSigningKeyCACert.crt
  restore_db
fi
}

pkits_BasicConstraints()
{
  break_table "NIST PKITS Section 4.6: Verifying Basic Constraints"

  VFY_ACTION="Invalid Missing basicConstraints Test1"; log_banner
  certImport MissingbasicConstraintsCACert
  crlImport MissingbasicConstraintsCACRL.crl
  pkitsn $certs/InvalidMissingbasicConstraintsTest1EE.crt \
      $certs/MissingbasicConstraintsCACert.crt
  restore_db

  VFY_ACTION="Invalid cA False Test2"; log_banner
  certImport basicConstraintsCriticalcAFalseCACert
  crlImport basicConstraintsCriticalcAFalseCACRL.crl
  pkitsn $certs/InvalidcAFalseTest2EE.crt \
      $certs/basicConstraintsCriticalcAFalseCACert.crt
  restore_db

  VFY_ACTION="Invalid cA False Test3"; log_banner
  certImport basicConstraintsNotCriticalcAFalseCACert
  crlImport basicConstraintsNotCriticalcAFalseCACRL.crl
  pkitsn $certs/InvalidcAFalseTest3EE.crt \
      $certs/basicConstraintsNotCriticalcAFalseCACert.crt
  restore_db

  VFY_ACTION="Valid basicConstraints Not Critical Test4"; log_banner
  certImport basicConstraintsNotCriticalCACert
  crlImport basicConstraintsNotCriticalCACRL.crl
  pkits $certs/ValidbasicConstraintsNotCriticalTest4EE.crt \
      $certs/basicConstraintsNotCriticalCACert.crt
  restore_db

  VFY_ACTION="Invalid pathLenConstraint Test5"; log_banner
  certImport pathLenConstraint0CACert
  crlImport pathLenConstraint0CACRL.crl
  certImport pathLenConstraint0subCACert
  crlImport pathLenConstraint0subCACRL.crl
  pkitsn $certs/InvalidpathLenConstraintTest5EE.crt \
      $certs/pathLenConstraint0subCACert.crt \
      $certs/pathLenConstraint0CACert.crt
  restore_db

  VFY_ACTION="Invalid pathLenConstraint Test6"; log_banner
  certImport pathLenConstraint0CACert
  crlImport pathLenConstraint0CACRL.crl
  certImport pathLenConstraint0subCACert
  crlImport pathLenConstraint0subCACRL.crl
  pkitsn $certs/InvalidpathLenConstraintTest6EE.crt \
      $certs/pathLenConstraint0subCACert.crt \
      $certs/pathLenConstraint0CACert.crt
  restore_db

  VFY_ACTION="Valid pathLenConstraint Test7"; log_banner
  certImport pathLenConstraint0CACert
  crlImport pathLenConstraint0CACRL.crl
  pkits $certs/ValidpathLenConstraintTest7EE.crt \
      $certs/pathLenConstraint0CACert.crt
  restore_db

  VFY_ACTION="Valid pathLenConstraint test8"; log_banner
  certImport pathLenConstraint0CACert
  crlImport pathLenConstraint0CACRL.crl
  pkits $certs/ValidpathLenConstraintTest8EE.crt \
      $certs/pathLenConstraint0CACert.crt
  restore_db

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
  restore_db

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
  restore_db

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
  restore_db

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
  restore_db

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
  restore_db

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
  restore_db

### bug 232737 ###
if [ -n "${KNOWN_BUG}" ]; then
  VFY_ACTION="Valid Self-Issued pathLenConstraint Test15"; log_banner
  certImport pathLenConstraint0CACert
  crlImport pathLenConstraint0CACRL.crl
  pkits $certs/ValidSelfIssuedpathLenConstraintTest15EE.crt \
      $certs/pathLenConstraint0SelfIssuedCACert.crt \
      $certs/pathLenConstraint0CACert.crt
  restore_db
fi

  VFY_ACTION="Invalid Self-Issued pathLenConstraint Test16"; log_banner
  certImport pathLenConstraint0CACert
  crlImport pathLenConstraint0CACRL.crl
  certImport pathLenConstraint0subCA2Cert
  crlImport pathLenConstraint0subCA2CRL.crl
  pkitsn $certs/InvalidSelfIssuedpathLenConstraintTest16EE.crt \
      $certs/pathLenConstraint0subCA2Cert.crt \
      $certs/pathLenConstraint0SelfIssuedCACert.crt \
      $certs/pathLenConstraint0CACert.crt
  restore_db

### bug 232737 ###
if [ -n "${KNOWN_BUG}" ]; then
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
  restore_db
fi
}

pkits_KeyUsage()
{
  break_table "NIST PKITS Section 4.7: Key Usage"

  VFY_ACTION="Invalid keyUsage Critical keyCertSign False Test1"; log_banner
  certImport keyUsageCriticalkeyCertSignFalseCACert
  crlImport keyUsageCriticalkeyCertSignFalseCACRL.crl
  pkitsn $certs/InvalidkeyUsageCriticalkeyCertSignFalseTest1EE.crt \
      $certs/keyUsageCriticalkeyCertSignFalseCACert.crt
  restore_db

  VFY_ACTION="Invalid keyUsage Not Critical keyCertSign False Test2"; log_banner
  certImport keyUsageNotCriticalkeyCertSignFalseCACert
  crlImport keyUsageNotCriticalkeyCertSignFalseCACRL.crl
  pkitsn $certs/InvalidkeyUsageNotCriticalkeyCertSignFalseTest2EE.crt \
      $certs/keyUsageNotCriticalkeyCertSignFalseCACert.crt
  restore_db

  VFY_ACTION="Valid keyUsage Not Critical Test3"; log_banner
  certImport keyUsageNotCriticalCACert
  crlImport keyUsageNotCriticalCACRL.crl
  pkits $certs/ValidkeyUsageNotCriticalTest3EE.crt \
      $certs/keyUsageNotCriticalCACert.crt
  restore_db

  VFY_ACTION="Invalid keyUsage Critical cRLSign False Test4"; log_banner
  certImport keyUsageCriticalcRLSignFalseCACert
  crlImportn keyUsageCriticalcRLSignFalseCACRL.crl
  if [ $RET -eq 0 ] ; then 
      pkitsn $certs/InvalidkeyUsageCriticalcRLSignFalseTest4EE.crt \
          $certs/keyUsageCriticalcRLSignFalseCACert.crt
  fi
  restore_db

  VFY_ACTION="Invalid keyUsage Not Critical cRLSign False Test5"; log_banner
  certImport keyUsageNotCriticalcRLSignFalseCACert
  crlImportn keyUsageNotCriticalcRLSignFalseCACRL.crl
  if [ $RET -eq 0 ] ; then 
      pkitsn $certs/InvalidkeyUsageNotCriticalcRLSignFalseTest5EE.crt \
          $certs/keyUsageNotCriticalcRLSignFalseCACert.crt
  fi
  restore_db
}

pkits_CertificatePolicies()
{
  break_table "NIST PKITS Section 4.8: Certificate Policies"

  VFY_ACTION="All Certificates Same Policy Test1"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkits $certs/ValidCertificatePathTest1EE.crt \
      $certs/GoodCACert.crt
  restore_db

  VFY_ACTION="All Certificates No Policies Test2"; log_banner
  certImport NoPoliciesCACert
  crlImport NoPoliciesCACRL.crl
  pkits $certs/AllCertificatesNoPoliciesTest2EE.crt \
      $certs/NoPoliciesCACert.crt
  restore_db

  VFY_ACTION="Different Policies Test3"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  certImport PoliciesP2subCACert
  crlImport PoliciesP2subCACRL.crl
  pkits $certs/DifferentPoliciesTest3EE.crt \
      $certs/PoliciesP2subCACert.crt \
      $certs/GoodCACert.crt
  restore_db

  VFY_ACTION="Different Policies Test4"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  certImport GoodsubCACert
  crlImport GoodsubCACRL.crl
  pkits $certs/DifferentPoliciesTest4EE.crt \
      $certs/GoodsubCACert.crt \
      $certs/GoodCACert.crt
  restore_db

  VFY_ACTION="Different Policies Test5"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  certImport PoliciesP2subCA2Cert
  crlImport PoliciesP2subCA2CRL.crl
  pkits $certs/DifferentPoliciesTest5EE.crt \
      $certs/PoliciesP2subCA2Cert.crt \
      $certs/GoodCACert.crt
  restore_db

  VFY_ACTION="Overlapping Policies Test6"; log_banner
  certImport PoliciesP1234CACert
  crlImport PoliciesP1234CACRL.crl
  certImport PoliciesP1234subCAP123Cert
  crlImport PoliciesP1234subCAP123CRL.crl
  certImport PoliciesP1234subsubCAP123P12Cert
  crlImport PoliciesP1234subsubCAP123P12CRL.crl
  pkits $certs/OverlappingPoliciesTest6EE.crt \
      $certs/PoliciesP1234subsubCAP123P12Cert.crt \
      $certs/PoliciesP1234subCAP123Cert.crt \
      $certs/PoliciesP1234CACert.crt
  restore_db

  VFY_ACTION="Different Policies Test7"; log_banner
  certImport PoliciesP123CACert
  crlImport PoliciesP123CACRL.crl
  certImport PoliciesP123subCAP12Cert
  crlImport PoliciesP123subCAP12CRL.crl
  certImport PoliciesP123subsubCAP12P1Cert
  crlImport PoliciesP123subsubCAP12P1CRL.crl
  pkits $certs/DifferentPoliciesTest7EE.crt \
      $certs/PoliciesP123subsubCAP12P1Cert.crt \
      $certs/PoliciesP123subCAP12Cert.crt \
      $certs/PoliciesP123CACert.crt
  restore_db

  VFY_ACTION="Different Policies Test8"; log_banner
  certImport PoliciesP12CACert
  crlImport PoliciesP12CACRL.crl
  certImport PoliciesP12subCAP1Cert
  crlImport PoliciesP12subCAP1CRL.crl
  certImport PoliciesP12subsubCAP1P2Cert
  crlImport PoliciesP12subsubCAP1P2CRL.crl
  pkits $certs/DifferentPoliciesTest8EE.crt \
      $certs/PoliciesP123subsubCAP12P1Cert.crt \
      $certs/PoliciesP12subCAP1Cert.crt \
      $certs/PoliciesP12CACert.crt
  restore_db

  VFY_ACTION="Different Policies Test9"; log_banner
  certImport PoliciesP123CACert
  crlImport PoliciesP123CACRL.crl
  certImport PoliciesP123subCAP12Cert
  crlImport PoliciesP123subCAP12CRL.crl
  certImport PoliciesP123subsubCAP12P2Cert
  crlImport PoliciesP123subsubCAP2P2CRL.crl
  certImport PoliciesP123subsubsubCAP12P2P1Cert
  crlImport PoliciesP123subsubsubCAP12P2P1CRL.crl
  pkits $certs/DifferentPoliciesTest9EE.crt \
      $certs/PoliciesP123subsubsubCAP12P2P1Cert.crt \
      $certs/PoliciesP123subsubCAP12P1Cert.crt \
      $certs/PoliciesP12subCAP1Cert.crt \
      $certs/PoliciesP12CACert.crt
  restore_db

  VFY_ACTION="All Certificates Same Policies Test10"; log_banner
  certImport PoliciesP12CACert
  crlImport PoliciesP12CACRL.crl
  pkits $certs/AllCertificatesSamePoliciesTest10EE.crt \
      $certs/NoPoliciesCACert.crt
  restore_db

  VFY_ACTION="All Certificates AnyPolicy Test11"; log_banner
  certImport anyPolicyCACert
  crlImport anyPolicyCACRL.crl
  pkits $certs/AllCertificatesanyPolicyTest11EE.crt \
      $certs/anyPolicyCACert.crt
  restore_db

  VFY_ACTION="Different Policies Test12"; log_banner
  certImport PoliciesP3CACert
  crlImport PoliciesP3CACRL.crl
  pkits $certs/DifferentPoliciesTest12EE.crt \
      $certs/PoliciesP3CACert.crt
  restore_db

  VFY_ACTION="All Certificates Same Policies Test13"; log_banner
  certImport PoliciesP123CACert
  crlImport PoliciesP123CACRL.crl
  pkits $certs/AllCertificatesSamePoliciesTest13EE.crt \
      $certs/PoliciesP123CACert.crt
  restore_db

  VFY_ACTION="AnyPolicy Test14"; log_banner
  certImport anyPolicyCACert
  crlImport anyPolicyCACRL.crl
  pkits $certs/AnyPolicyTest14EE.crt \
      $certs/anyPolicyCACert.crt
  restore_db

  VFY_ACTION="User Notice Qualifier Test15"; log_banner
  pkits $certs/UserNoticeQualifierTest15EE.crt

  VFY_ACTION="User Notice Qualifier Test16"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkits $certs/UserNoticeQualifierTest16EE.crt \
      $certs/GoodCACert.crt

  VFY_ACTION="User Notice Qualifier Test17"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkits $certs/UserNoticeQualifierTest17EE.crt \
      $certs/GoodCACert.crt
  restore_db

  VFY_ACTION="User Notice Qualifier Test18"; log_banner
  certImport PoliciesP12CACert
  crlImport PoliciesP12CACRL.crl
  pkits $certs/UserNoticeQualifierTest18EE.crt \
      $certs/PoliciesP12CACert.crt
  restore_db

  VFY_ACTION="User Notice Qualifier Test19"; log_banner
  pkits $certs/UserNoticeQualifierTest19EE.crt

  VFY_ACTION="CPS Pointer Qualifier Test20"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  pkits $certs/CPSPointerQualifierTest20EE.crt \
      $certs/GoodCACert.crt
  restore_db
}

pkits_RequireExplicitPolicy()
{
  break_table "NIST PKITS Section 4.9: Require Explicit Policy"

  VFY_ACTION="Valid RequireExplicitPolicy Test1"; log_banner
  certImportn requireExplicitPolicy10CACert
  crlImportn requireExplicitPolicy10CACRL.crl
  certImport requireExplicitPolicy10subCACert
  crlImport requireExplicitPolicy10subCACRL.crl
  certImport requireExplicitPolicy10subsubCACert
  crlImport requireExplicitPolicy10subsubCACRL.crl
  certImport requireExplicitPolicy10subsubsubCACert
  crlImport requireExplicitPolicy10subsubsubCACRL.crl
  pkits $certs/ValidrequireExplicitPolicyTest1EE.crt \
      $certs/requireExplicitPolicy10subsubsubCACert.crt \
      $certs/requireExplicitPolicy10subsubCACert.crt \
      $certs/requireExplicitPolicy10subCACert.crt \
      $certs/requireExplicitPolicy10CACert.crt
  restore_db

  VFY_ACTION="Valid RequireExplicitPolicy Test2"; log_banner
  certImportn requireExplicitPolicy5CACert
  crlImportn requireExplicitPolicy5CACRL.crl
  certImport requireExplicitPolicy5subCACert
  crlImport requireExplicitPolicy5subCACRL.crl
  certImport requireExplicitPolicy5subsubCACert
  crlImport requireExplicitPolicy5subsubCACRL.crl
  certImport requireExplicitPolicy5subsubsubCACert
  crlImport requireExplicitPolicy5subsubsubCACRL.crl
  pkits $certs/ValidrequireExplicitPolicyTest2EE.crt \
      $certs/requireExplicitPolicy5subsubsubCACert.crt \
      $certs/requireExplicitPolicy5subsubCACert.crt \
      $certs/requireExplicitPolicy5subCACert.crt \
      $certs/requireExplicitPolicy5CACert.crt
  restore_db

  VFY_ACTION="Invalid RequireExplicitPolicy Test3"; log_banner
  certImportn requireExplicitPolicy4CACert
  crlImportn requireExplicitPolicy4CACRL.crl
  certImport requireExplicitPolicy4subCACert
  crlImport requireExplicitPolicy4subCACRL.crl
  certImport requireExplicitPolicy4subsubCACert
  crlImport requireExplicitPolicy4subsubCACRL.crl
  certImport requireExplicitPolicy4subsubsubCACert
  crlImport requireExplicitPolicy4subsubsubCACRL.crl
  pkitsn $certs/InvalidrequireExplicitPolicyTest3EE.crt \
      $certs/requireExplicitPolicy4subsubsubCACert.crt \
      $certs/requireExplicitPolicy4subsubCACert.crt \
      $certs/requireExplicitPolicy4subCACert.crt \
      $certs/requireExplicitPolicy4CACert.crt
  restore_db

  VFY_ACTION="Valid RequireExplicitPolicy Test4"; log_banner
  certImportn requireExplicitPolicy0CACert
  crlImportn requireExplicitPolicy0CACRL.crl
  certImport requireExplicitPolicy0subCACert
  crlImport requireExplicitPolicy0subCACRL.crl
  certImport requireExplicitPolicy0subsubCACert
  crlImport requireExplicitPolicy0subsubCACRL.crl
  certImport requireExplicitPolicy0subsubsubCACert
  crlImport requireExplicitPolicy0subsubsubCACRL.crl
  pkits $certs/ValidrequireExplicitPolicyTest4EE.crt \
      $certs/requireExplicitPolicy0subsubsubCACert.crt \
      $certs/requireExplicitPolicy0subsubCACert.crt \
      $certs/requireExplicitPolicy0subCACert.crt \
      $certs/requireExplicitPolicy0CACert.crt
  restore_db

  VFY_ACTION="Invalid RequireExplicitPolicy Test5"; log_banner
  certImportn requireExplicitPolicy7CACert
  crlImportn requireExplicitPolicy7CACRL.crl
  certImportn requireExplicitPolicy7subCARE2Cert
  crlImportn requireExplicitPolicy7subCARE2CRL.crl
  certImportn requireExplicitPolicy7subsubCARE2RE4Cert
  crlImportn requireExplicitPolicy7subsubCARE2RE4CRL.crl
  certImport requireExplicitPolicy7subsubsubCARE2RE4Cert
  crlImport requireExplicitPolicy7subsubsubCARE2RE4CRL.crl
  pkitsn $certs/InvalidrequireExplicitPolicyTest5EE.crt \
      $certs/requireExplicitPolicy7subsubsubCARE2RE4Cert.crt \
      $certs/requireExplicitPolicy7subsubCARE2RE4Cert.crt \
      $certs/requireExplicitPolicy7subCARE2Cert.crt \
      $certs/requireExplicitPolicy7CACert.crt
  restore_db

  VFY_ACTION="Valid Self-Issued RequireExplicitPolicy Test6"; log_banner
  certImportn requireExplicitPolicy2CACert
  crlImportn requireExplicitPolicy2CACRL.crl
  pkits $certs/ValidSelfIssuedrequireExplicitPolicyTest6EE.crt \
      $certs/requireExplicitPolicy2SelfIssuedCACert.crt \
      $certs/requireExplicitPolicy2CACert.crt
  restore_db

  VFY_ACTION="Invalid Self-Issued RequireExplicitPolicy Test7"; log_banner
  certImportn requireExplicitPolicy2CACert
  crlImportn requireExplicitPolicy2CACRL.crl
  certImport requireExplicitPolicy2subCACert
  crlImport requireExplicitPolicy2subCACRL.crl
  pkitsn $certs/InvalidSelfIssuedrequireExplicitPolicyTest7EE.crt \
      $certs/requireExplicitPolicy2subCACert.crt \
      $certs/requireExplicitPolicy2SelfIssuedCACert.crt \
      $certs/requireExplicitPolicy2CACert.crt
  restore_db

  VFY_ACTION="Invalid Self-Issued RequireExplicitPolicy Test8"; log_banner
  certImportn requireExplicitPolicy2CACert
  crlImportn requireExplicitPolicy2CACRL.crl
  certImport requireExplicitPolicy2subCACert
  crlImport requireExplicitPolicy2subCACRL.crl
  pkitsn $certs/InvalidSelfIssuedrequireExplicitPolicyTest8EE.crt \
      $certs/requireExplicitPolicy2SelfIssuedsubCACert.crt \
      $certs/requireExplicitPolicy2subCACert.crt \
      $certs/requireExplicitPolicy2SelfIssuedCACert.crt \
      $certs/requireExplicitPolicy2CACert.crt
  restore_db
}

pkits_PolicyMappings()
{
  break_table "NIST PKITS Section 4.10: Policy Mappings"

  VFY_ACTION="Valid Policy Mapping Test1"; log_banner
  certImportn Mapping1to2CACert
  crlImportn Mapping1to2CACRL.crl
  pkits $certs/ValidPolicyMappingTest1EE.crt \
      $certs/Mapping1to2CACert.crt
  restore_db

  VFY_ACTION="Invalid Policy Mapping Test2"; log_banner
  certImportn Mapping1to2CACert
  crlImportn Mapping1to2CACRL.crl
  pkitsn $certs/InvalidPolicyMappingTest2EE.crt \
      $certs/Mapping1to2CACert.crt
  restore_db

  VFY_ACTION="Valid Policy Mapping Test3"; log_banner
  certImportn P12Mapping1to3CACert
  crlImportn P12Mapping1to3CACRL.crl
  certImportn P12Mapping1to3subCACert
  crlImportn P12Mapping1to3subCACRL.crl
  certImportn P12Mapping1to3subsubCACert
  crlImportn P12Mapping1to3subsubCACRL.crl
  pkits $certs/ValidPolicyMappingTest3EE.crt \
      $certs/P12Mapping1to3subsubCACert.crt \
      $certs/P12Mapping1to3subCACert.crt \
      $certs/P12Mapping1to3CA.crt
  restore_db

  VFY_ACTION="Invalid Policy Mapping Test4"; log_banner
  certImportn P12Mapping1to3CACert
  crlImportn P12Mapping1to3CACRL.crl
  certImportn P12Mapping1to3subCACert
  crlImportn P12Mapping1to3subCACRL.crl
  certImportn P12Mapping1to3subsubCACert
  crlImportn P12Mapping1to3subsubCACRL.crl
  pkitsn $certs/InvalidPolicyMappingTest4EE.crt \
      $certs/P12Mapping1to3subsubCACert.crt \
      $certs/P12Mapping1to3subCACert.crt \
      $certs/P12Mapping1to3CA.crt
  restore_db

  VFY_ACTION="Valid Policy Mapping Test5"; log_banner
  certImportn P1Mapping1to234CACert
  crlImportn P1Mapping1to234CACRL.crl
  certImportn P1Mapping1to234subCACert
  crlImportn P1Mapping1to234subCACRL.crl
  pkits $certs/ValidPolicyMappingTest5EE.crt \
      $certs/P1Mapping1to234subCACert.crt \
      $certs/P1Mapping1to234CA.crt
  restore_db

  VFY_ACTION="Valid Policy Mapping Test6"; log_banner
  certImportn P1Mapping1to234CACert
  crlImportn P1Mapping1to234CACRL.crl
  certImportn P1Mapping1to234subCACert
  crlImportn P1Mapping1to234subCACRL.crl
  pkits $certs/ValidPolicyMappingTest6EE.crt \
      $certs/P1Mapping1to234subCACert.crt \
      $certs/P1Mapping1to234CA.crt
  restore_db

  VFY_ACTION="Invalid Mapping from anyPolicy Test7"; log_banner
  certImportn MappingFromanyPolicyCACert
  crlImportn MappingFromanyPolicyCACRL.crl
  pkitsn $certs/InvalidMappingFromanyPolicyTest7EE.crt \
      $certs/MappingFromanyPolicyCACert.crt
  restore_db

  VFY_ACTION="Invalid Mapping to anyPolicy Test8"; log_banner
  certImportn MappingToanyPolicyCACert
  crlImportn MappingToanyPolicyCACRL.crl
  pkitsn $certs/InvalidMappingToanyPolicyTest8EE.crt \
      $certs/MappingToanyPolicyCACert.crt
  restore_db

  VFY_ACTION="Valid Policy Mapping Test9"; log_banner
  certImport PanyPolicyMapping1to2CACert
  crlImport PanyPolicyMapping1to2CACRL.crl
  pkits $certs/ValidPolicyMappingTest9EE.crt \
      $certs/PanyPolicyMapping1to2CACert.crt
  restore_db

  VFY_ACTION="Invalid Policy Mapping Test10"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  certImportn GoodsubCAPanyPolicyMapping1to2CACert
  crlImportn GoodsubCAPanyPolicyMapping1to2CACRL.crl
  pkitsn $certs/InvalidPolicyMappingTest10EE.crt \
      $certs/GoodsubCAPanyPolicyMapping1to2CACert.crt \
      $certs/GoodCACert.crt
  restore_db

  VFY_ACTION="Valid Policy Mapping Test11"; log_banner
  certImport GoodCACert
  crlImport GoodCACRL.crl
  certImportn GoodsubCAPanyPolicyMapping1to2CACert
  crlImportn GoodsubCAPanyPolicyMapping1to2CACRL.crl
  pkits $certs/ValidPolicyMappingTest11EE.crt \
      $certs/GoodsubCAPanyPolicyMapping1to2CACert.crt \
      $certs/GoodCACert.crt
  restore_db

  VFY_ACTION="Valid Policy Mapping Test12"; log_banner
  certImportn P12Mapping1to3CACert
  crlImportn P12Mapping1to3CACRL.crl
  pkits $certs/ValidPolicyMappingTest12EE.crt \
      $certs/P12Mapping1to3CACert.crt
  restore_db

  VFY_ACTION="Valid Policy Mapping Test13"; log_banner
  certImportn P1anyPolicyMapping1to2CACert
  crlImportn P1anyPolicyMapping1to2CACRL.crl
  pkits $certs/ValidPolicyMappingTest13EE.crt \
      $certs/P1anyPolicyMapping1to2CACert.crt
  restore_db

  VFY_ACTION="Valid Policy Mapping Test14"; log_banner
  certImportn P1anyPolicyMapping1to2CACert
  crlImportn P1anyPolicyMapping1to2CACRL.crl
  pkits $certs/ValidPolicyMappingTest14EE.crt \
      $certs/P1anyPolicyMapping1to2CACert.crt
  restore_db
}


pkits_InhibitPolicyMapping()
{
  break_table "NIST PKITS Section 4.11: Inhibit Policy Mapping"

  VFY_ACTION="Invalid inhibitPolicyMapping Test1"; log_banner
  certImportn inhibitPolicyMapping0CACert
  crlImportn inhibitPolicyMapping0CACRL.crl
  certImportn inhibitPolicyMapping0subCACert
  crlImportn inhibitPolicyMapping0subCACRL.crl
  pkitsn $certs/InvalidinhibitPolicyMappingTest1EE.crt \
      $certs/inhibitPolicyMapping0CACert.crt \
      $certs/inhibitPolicyMapping0subCACert.crt
  restore_db

  VFY_ACTION="Valid inhibitPolicyMapping Test2"; log_banner
  certImportn inhibitPolicyMapping1P12CACert
  crlImportn inhibitPolicyMapping1P12CACRL.crl
  certImportn inhibitPolicyMapping1P12subCACert
  crlImportn inhibitPolicyMapping1P12subCACRL.crl
  pkits $certs/ValidinhibitPolicyMappingTest2EE.crt \
      $certs/inhibitPolicyMapping1P12CACert.crt \
      $certs/inhibitPolicyMapping1P12subCACert.crt
  restore_db

  VFY_ACTION="Invalid inhibitPolicyMapping Test3"; log_banner
  certImportn inhibitPolicyMapping1P12CACert
  crlImportn inhibitPolicyMapping1P12CACRL.crl
  certImportn inhibitPolicyMapping1P12subCACert
  crlImportn inhibitPolicyMapping1P12subCACRL.crl
  certImportn inhibitPolicyMapping1P12subsubCACert
  crlImportn inhibitPolicyMapping1P12subsubCACRL.crl
  pkitsn $certs/InvalidinhibitPolicyMappingTest3EE.crt \
      $certs/inhibitPolicyMapping1P12subsubCACert.crt \
      $certs/inhibitPolicyMapping1P12subCACert.crt \
      $certs/inhibitPolicyMapping1P12CACert.crt
  restore_db

  VFY_ACTION="Valid inhibitPolicyMapping Test4"; log_banner
  certImportn inhibitPolicyMapping1P12CACert
  crlImportn inhibitPolicyMapping1P12CACRL.crl
  certImportn inhibitPolicyMapping1P12subCACert
  crlImportn inhibitPolicyMapping1P12subCACRL.crl
  certImportn inhibitPolicyMapping1P12subsubCACert
  crlImportn inhibitPolicyMapping1P12subsubCACRL.crl
  pkits $certs/ValidinhibitPolicyMappingTest4EE.crt \
      $certs/inhibitPolicyMapping1P12CACert.crt \
      $certs/inhibitPolicyMapping1P12subCACert.crt
  restore_db

  VFY_ACTION="Invalid inhibitPolicyMapping Test5"; log_banner
  certImportn inhibitPolicyMapping5CACert
  crlImportn inhibitPolicyMapping5CACRL.crl
  certImportn inhibitPolicyMapping5subCACert
  crlImportn inhibitPolicyMapping5subCACRL.crl
  certImport inhibitPolicyMapping5subsubCACert
  crlImport inhibitPolicyMapping5subsubCACRL.crl
  pkitsn $certs/InvalidinhibitPolicyMappingTest5EE.crt \
      $certs/inhibitPolicyMapping5subsubCACert.crt \
      $certs/inhibitPolicyMapping5subCACert.crt \
      $certs/inhibitPolicyMapping5CACert.crt
  restore_db

  VFY_ACTION="Invalid inhibitPolicyMapping Test6"; log_banner
  certImportn inhibitPolicyMapping1P12CACert
  crlImportn inhibitPolicyMapping1P12CACRL.crl
  certImportn inhibitPolicyMapping1P12subCAIPM5Cert
  crlImportn inhibitPolicyMapping1P12subCAIPM5CRL.crl
  certImport inhibitPolicyMapping1P12subsubCAIPM5Cert
  crlImportn inhibitPolicyMapping1P12subsubCAIPM5CRL.crl
  pkitsn $certs/InvalidinhibitPolicyMappingTest6EE.crt \
      $certs/inhibitPolicyMapping1P12subsubCAIPM5Cert.crt \
      $certs/inhibitPolicyMapping1P12subCAIPM5Cert.crt \
      $certs/inhibitPolicyMapping1P12CACert.crt
  restore_db

  VFY_ACTION="Valid Self-Issued inhibitPolicyMapping Test7"; log_banner
  certImportn inhibitPolicyMapping1P1CACert
  crlImportn inhibitPolicyMapping1P1CACRL.crl
  certImportn inhibitPolicyMapping1P1subCACert
  crlImportn inhibitPolicyMapping1P1subCACRL.crl
  pkits $certs/ValidSelfIssuedinhibitPolicyMappingTest7EE.crt \
      $certs/inhibitPolicyMapping1P1subCACert.crt \
      $certs/inhibitPolicyMapping1P1SelfIssuedCACert.crt \
      $certs/inhibitPolicyMapping1P1CACert.crt
  restore_db

  VFY_ACTION="Invalid Self-Issued inhibitPolicyMapping Test8"; log_banner
  certImportn inhibitPolicyMapping1P1CACert
  crlImportn inhibitPolicyMapping1P1CACRL.crl
  certImportn inhibitPolicyMapping1P1subCACert
  crlImportn inhibitPolicyMapping1P1subCACRL.crl
  certImport inhibitPolicyMapping1P1subsubCACert
  crlImportn inhibitPolicyMapping1P1subsubCACRL.crl
  pkitsn $certs/InvalidSelfIssuedinhibitPolicyMappingTest8EE.crt \
      $certs/inhibitPolicyMapping1P1subsubCACert.crt \
      $certs/inhibitPolicyMapping1P1subCACert.crt \
      $certs/inhibitPolicyMapping1P1SelfIssuedCACert.crt \
      $certs/inhibitPolicyMapping1P1CACert.crt
  restore_db

  VFY_ACTION="Invalid Self-Issued inhibitPolicyMapping Test9"; log_banner
  certImportn inhibitPolicyMapping1P1CACert
  crlImportn inhibitPolicyMapping1P1CACRL.crl
  certImportn inhibitPolicyMapping1P1subCACert
  crlImportn inhibitPolicyMapping1P1subCACRL.crl
  certImportn inhibitPolicyMapping1P1subsubCACert
  crlImportn inhibitPolicyMapping1P1subsubCACRL.crl
  pkitsn $certs/InvalidSelfIssuedinhibitPolicyMappingTest9EE.crt \
      $certs/inhibitPolicyMapping1P1subsubCACert.crt \
      $certs/inhibitPolicyMapping1P1subCACert.crt \
      $certs/inhibitPolicyMapping1P1SelfIssuedCACert.crt \
      $certs/inhibitPolicyMapping1P1CACert.crt
  restore_db

  VFY_ACTION="Invalid Self-Issued inhibitPolicyMapping Test10"; log_banner
  certImportn inhibitPolicyMapping1P1CACert
  crlImportn inhibitPolicyMapping1P1CACRL.crl
  certImportn inhibitPolicyMapping1P1subCACert
  crlImportn inhibitPolicyMapping1P1subCACRL.crl
  pkitsn $certs/InvalidSelfIssuedinhibitPolicyMappingTest10EE.crt \
      $certs/inhibitPolicyMapping1P1SelfIssuedsubCACert.crt \
      $certs/inhibitPolicyMapping1P1subCACert.crt \
      $certs/inhibitPolicyMapping1P1SelfIssuedCACert.crt \
      $certs/inhibitPolicyMapping1P1CACert.crt
  restore_db

  VFY_ACTION="Invalid Self-Issued inhibitPolicyMapping Test11"; log_banner
  certImportn inhibitPolicyMapping1P1CACert
  crlImportn inhibitPolicyMapping1P1CACRL.crl
  certImportn inhibitPolicyMapping1P1subCACert
  crlImportn inhibitPolicyMapping1P1subCACRL.crl
  pkitsn $certs/InvalidSelfIssuedinhibitPolicyMappingTest11EE.crt \
      $certs/inhibitPolicyMapping1P1SelfIssuedsubCACert.crt \
      $certs/inhibitPolicyMapping1P1subCACert.crt \
      $certs/inhibitPolicyMapping1P1SelfIssuedCACert.crt \
      $certs/inhibitPolicyMapping1P1CACert.crt
  restore_db
}


pkits_InhibitAnyPolicy()
{
  break_table "NIST PKITS Section 4.12: Inhibit Any Policy"

  VFY_ACTION="Invalid inhibitAnyPolicy Test1"; log_banner
  certImportn inhibitAnyPolicy0CACert
  crlImportn inhibitAnyPolicy0CACRL.crl
  pkitsn $certs/InvalidinhibitAnyPolicyTest1EE.crt \
      $certs/inhibitAnyPolicy0CACert.crt
  restore_db

  VFY_ACTION="Valid inhibitAnyPolicy Test2"; log_banner
  certImportn inhibitAnyPolicy0CACert
  crlImportn inhibitAnyPolicy0CACRL.crl
  pkits $certs/ValidinhibitAnyPolicyTest2EE.crt \
      $certs/inhibitAnyPolicy0CACert.crt
  restore_db

  VFY_ACTION="inhibitAnyPolicy Test3"; log_banner
  certImportn inhibitAnyPolicy1CACert
  crlImportn inhibitAnyPolicy1CACRL.crl
  certImport inhibitAnyPolicy1subCA1Cert
  crlImport inhibitAnyPolicy1subCA1CRL.crl
  pkits $certs/inhibitAnyPolicyTest3EE.crt \
      $certs/inhibitAnyPolicy1CACert.crt \
      $certs/inhibitAnyPolicy1subCA1Cert.crt
  restore_db

  VFY_ACTION="Invalid inhibitAnyPolicy Test4"; log_banner
  certImportn inhibitAnyPolicy1CACert
  crlImportn inhibitAnyPolicy1CACRL.crl
  certImport inhibitAnyPolicy1subCA1Cert
  crlImport inhibitAnyPolicy1subCA1CRL.crl
  pkitsn $certs/InvalidinhibitAnyPolicyTest4EE.crt \
      $certs/inhibitAnyPolicy1CACert.crt \
      $certs/inhibitAnyPolicy1subCA1Cert.crt
  restore_db

  VFY_ACTION="Invalid inhibitAnyPolicy Test5"; log_banner
  certImportn inhibitAnyPolicy5CACert
  crlImportn inhibitAnyPolicy5CACRL.crl
  certImportn inhibitAnyPolicy5subCACert
  crlImportn inhibitAnyPolicy5subCACRL.crl
  certImport inhibitAnyPolicy5subsubCACert
  crlImport inhibitAnyPolicy5subsubCACRL.crl
  pkitsn $certs/InvalidinhibitAnyPolicyTest5EE.crt \
      $certs/inhibitAnyPolicy5CACert.crt \
      $certs/inhibitAnyPolicy5subCACert.crt \
      $certs/inhibitAnyPolicy5subsubCACert.crt
  restore_db

  VFY_ACTION="Invalid inhibitAnyPolicy Test6"; log_banner
  certImportn inhibitAnyPolicy1CACert
  crlImportn inhibitAnyPolicy1CACRL.crl
  certImportn inhibitAnyPolicy1subCAIAP5Cert
  crlImportn inhibitAnyPolicy1subCAIAP5CRL.crl
  pkitsn $certs/InvalidinhibitAnyPolicyTest5EE.crt \
      $certs/inhibitAnyPolicy1CACert.crt \
      $certs/inhibitAnyPolicy5subCACert.crt \
      $certs/inhibitAnyPolicy5subsubCACert.crt
  restore_db

  VFY_ACTION="Valid Self-Issued inhibitAnyPolicy Test7"; log_banner
  certImportn inhibitAnyPolicy1CACert
  crlImportn inhibitAnyPolicy1CACRL.crl
  certImport inhibitAnyPolicy1subCA2Cert
  crlImport inhibitAnyPolicy1subCA2CRL.crl
  pkits $certs/ValidSelfIssuedinhibitAnyPolicyTest7EE.crt \
      $certs/inhibitAnyPolicy1CACert.crt \
      $certs/inhibitAnyPolicy1SelfIssuedCACert.crt \
      $certs/inhibitAnyPolicy1subCA2Cert.crt
  restore_db

  VFY_ACTION="Invalid Self-Issued inhibitAnyPolicy Test8"; log_banner
  certImportn inhibitAnyPolicy1CACert
  crlImportn inhibitAnyPolicy1CACRL.crl
  certImport inhibitAnyPolicy1subCA2Cert
  crlImport inhibitAnyPolicy1subCA2CRL.crl
  certImport inhibitAnyPolicy1subsubCA2Cert
  crlImport inhibitAnyPolicy1subsubCA2CRL.crl
  pkitsn $certs/InvalidSelfIssuedinhibitAnyPolicyTest8EE.crt \
      $certs/inhibitAnyPolicy1CACert.crt \
      $certs/inhibitAnyPolicy1SelfIssuedCACert.crt \
      $certs/inhibitAnyPolicy1subCA2Cert.crt \
      $certs/inhibitAnyPolicy1subsubCA2Cert.crt
  restore_db

  VFY_ACTION="Valid Self-Issued inhibitAnyPolicy Test9"; log_banner
  certImportn inhibitAnyPolicy1CACert
  crlImportn inhibitAnyPolicy1CACRL.crl
  certImport inhibitAnyPolicy1subCA2Cert
  crlImport inhibitAnyPolicy1subCA2CRL.crl
  pkits $certs/ValidSelfIssuedinhibitAnyPolicyTest9EE.crt \
      $certs/inhibitAnyPolicy1CACert.crt \
      $certs/inhibitAnyPolicy1SelfIssuedCACert.crt \
      $certs/inhibitAnyPolicy1subCA2Cert.crt \
      $certs/inhibitAnyPolicy1SelfIssuedsubCA2Cert.crt
  restore_db

  VFY_ACTION="Invalid Self-Issued inhibitAnyPolicy Test10"; log_banner
  certImportn inhibitAnyPolicy1CACert
  crlImportn inhibitAnyPolicy1CACRL.crl
  certImport inhibitAnyPolicy1subCA2Cert
  crlImport inhibitAnyPolicy1subCA2CRL.crl
  pkitsn $certs/InvalidSelfIssuedinhibitAnyPolicyTest10EE.crt \
      $certs/inhibitAnyPolicy1CACert.crt \
      $certs/inhibitAnyPolicy1SelfIssuedCACert.crt \
      $certs/inhibitAnyPolicy1subCA2Cert.crt
  restore_db
}


pkits_NameConstraints()
{
  break_table "NIST PKITS Section 4.13: Name Constraints"

  VFY_ACTION="Valid DN nameConstraints Test1"; log_banner
  certImport nameConstraintsDN1CACert
  crlImport nameConstraintsDN1CACRL.crl
  pkits $certs/ValidDNnameConstraintsTest1EE.crt \
      $certs/nameConstraintsDN1CACert.crt
  restore_db

  VFY_ACTION="Invalid DN nameConstraints Test2"; log_banner
  certImport nameConstraintsDN1CACert
  crlImport nameConstraintsDN1CACRL.crl
  pkitsn $certs/InvalidDNnameConstraintsTest2EE.crt \
      $certs/nameConstraintsDN1CACert.crt
  restore_db

  VFY_ACTION="Invalid DN nameConstraints Test3"; log_banner
  certImport nameConstraintsDN1CACert
  crlImport nameConstraintsDN1CACRL.crl
  pkitsn $certs/InvalidDNnameConstraintsTest3EE.crt \
      $certs/nameConstraintsDN1CACert.crt
  restore_db

  VFY_ACTION="Valid DN nameConstraints Test4"; log_banner
  certImport nameConstraintsDN1CACert
  crlImport nameConstraintsDN1CACRL.crl
  pkits $certs/ValidDNnameConstraintsTest4EE.crt \
      $certs/nameConstraintsDN1CACert.crt
  restore_db

  VFY_ACTION="Valid DN nameConstraints Test5"; log_banner
  certImport nameConstraintsDN2CACert
  crlImport nameConstraintsDN2CACRL.crl
  pkits $certs/ValidDNnameConstraintsTest5EE.crt \
      $certs/nameConstraintsDN2CACert.crt
  restore_db

  VFY_ACTION="Valid DN nameConstraints Test6"; log_banner
  certImport nameConstraintsDN3CACert
  crlImport nameConstraintsDN3CACRL.crl
  pkits $certs/ValidDNnameConstraintsTest6EE.crt \
      $certs/nameConstraintsDN3CACert.crt
  restore_db

  VFY_ACTION="Invalid DN nameConstraints Test7"; log_banner
  certImport nameConstraintsDN3CACert
  crlImport nameConstraintsDN3CACRL.crl
  pkitsn $certs/InvalidDNnameConstraintsTest7EE.crt \
      $certs/nameConstraintsDN3CACert.crt
  restore_db

  VFY_ACTION="Invalid DN nameConstraints Test8"; log_banner
  certImport nameConstraintsDN4CACert
  crlImport nameConstraintsDN4CACRL.crl
  pkitsn $certs/InvalidDNnameConstraintsTest8EE.crt \
      $certs/nameConstraintsDN4CACert.crt
  restore_db

  VFY_ACTION="Invalid DN nameConstraints Test9"; log_banner
  certImport nameConstraintsDN4CACert
  crlImport nameConstraintsDN4CACRL.crl
  pkitsn $certs/InvalidDNnameConstraintsTest9EE.crt \
      $certs/nameConstraintsDN4CACert.crt
  restore_db

  VFY_ACTION="Invalid DN nameConstraints Test10"; log_banner
  certImport nameConstraintsDN5CACert
  crlImport nameConstraintsDN5CACRL.crl
  pkitsn $certs/InvalidDNnameConstraintsTest10EE.crt \
      $certs/nameConstraintsDN5CACert.crt
  restore_db

  VFY_ACTION="Valid DN nameConstraints Test11"; log_banner
  certImport nameConstraintsDN5CACert
  crlImport nameConstraintsDN5CACRL.crl
  pkits $certs/ValidDNnameConstraintsTest11EE.crt \
      $certs/nameConstraintsDN5CACert.crt
  restore_db

  VFY_ACTION="Invalid DN nameConstraints Test12"; log_banner
  certImport nameConstraintsDN1CACert
  crlImport nameConstraintsDN1CACRL.crl
  certImport nameConstraintsDN1subCA1Cert
  crlImport nameConstraintsDN1subCA1CRL.crl
  pkitsn $certs/InvalidDNnameConstraintsTest12EE.crt \
      $certs/nameConstraintsDN1subCA1Cert.crt \
      $certs/nameConstraintsDN1CACert.crt
  restore_db

  VFY_ACTION="Invalid DN nameConstraints Test13"; log_banner
  certImport nameConstraintsDN1CACert
  crlImport nameConstraintsDN1CACRL.crl
  certImport nameConstraintsDN1subCA2Cert
  crlImport nameConstraintsDN1subCA2CRL.crl
  pkitsn $certs/InvalidDNnameConstraintsTest13EE.crt \
      $certs/nameConstraintsDN1subCA2Cert.crt \
      $certs/nameConstraintsDN1CACert.crt
  restore_db

  VFY_ACTION="Valid DN nameConstraints Test14"; log_banner
  certImport nameConstraintsDN1CACert
  crlImport nameConstraintsDN1CACRL.crl
  certImport nameConstraintsDN1subCA2Cert
  crlImport nameConstraintsDN1subCA2CRL.crl
  pkits $certs/ValidDNnameConstraintsTest14EE.crt \
      $certs/nameConstraintsDN1subCA2Cert.crt \
      $certs/nameConstraintsDN1CACert.crt
  restore_db

  VFY_ACTION="Invalid DN nameConstraints Test15"; log_banner
  certImport nameConstraintsDN3CACert
  crlImport nameConstraintsDN3CACRL.crl
  certImport nameConstraintsDN3subCA1Cert
  crlImport nameConstraintsDN3subCA1CRL.crl
  pkitsn $certs/InvalidDNnameConstraintsTest15EE.crt \
      $certs/nameConstraintsDN3subCA1Cert.crt \
      $certs/nameConstraintsDN3CACert.crt
  restore_db

  VFY_ACTION="Invalid DN nameConstraints Test16"; log_banner
  certImport nameConstraintsDN3CACert
  crlImport nameConstraintsDN3CACRL.crl
  certImport nameConstraintsDN3subCA1Cert
  crlImport nameConstraintsDN3subCA1CRL.crl
  pkitsn $certs/InvalidDNnameConstraintsTest16EE.crt \
      $certs/nameConstraintsDN3subCA1Cert.crt \
      $certs/nameConstraintsDN3CACert.crt
  restore_db

  VFY_ACTION="Invalid DN nameConstraints Test17"; log_banner
  certImport nameConstraintsDN3CACert
  crlImport nameConstraintsDN3CACRL.crl
  certImport nameConstraintsDN3subCA2Cert
  crlImport nameConstraintsDN3subCA2CRL.crl
  pkitsn $certs/InvalidDNnameConstraintsTest17EE.crt \
      $certs/nameConstraintsDN3subCA2Cert.crt \
      $certs/nameConstraintsDN3CACert.crt
  restore_db

  VFY_ACTION="Valid DN nameConstraints Test18"; log_banner
  certImport nameConstraintsDN3CACert
  crlImport nameConstraintsDN3CACRL.crl
  certImport nameConstraintsDN3subCA2Cert
  crlImport nameConstraintsDN3subCA2CRL.crl
  pkits $certs/ValidDNnameConstraintsTest18EE.crt \
      $certs/nameConstraintsDN3subCA2Cert.crt \
      $certs/nameConstraintsDN3CACert.crt
  restore_db

### bug 232737 ###
if [ -n "${KNOWN_BUG}" ]; then
  VFY_ACTION="Valid Self-Issued DN nameConstraints Test19"; log_banner
  certImport nameConstraintsDN1CACert
  crlImport nameConstraintsDN1CACRL.crl
  pkits $certs/ValidDNnameConstraintsTest19EE.crt \
      $certs/nameConstraintsDN1SelfIssuedCACert.crt \
      $certs/nameConstraintsDN1CACert.crt
  restore_db
fi

  VFY_ACTION="Invalid Self-Issued DN nameConstraints Test20"; log_banner
  certImport nameConstraintsDN1CACert
  crlImport nameConstraintsDN1CACRL.crl
  pkitsn $certs/InvalidDNnameConstraintsTest20EE.crt \
      $certs/nameConstraintsDN1CACert.crt
  restore_db

  VFY_ACTION="Valid RFC822 nameConstraints Test21"; log_banner
  certImport nameConstraintsRFC822CA1Cert
  crlImport nameConstraintsRFC822CA1CRL.crl
  pkits $certs/ValidRFC822nameConstraintsTest21EE.crt \
      $certs/nameConstraintsRFC822CA1Cert.crt
  restore_db

  VFY_ACTION="Invalid RFC822 nameConstraints Test22"; log_banner
  certImport nameConstraintsRFC822CA1Cert
  crlImport nameConstraintsRFC822CA1CRL.crl
  pkitsn $certs/InvalidRFC822nameConstraintsTest22EE.crt \
      $certs/nameConstraintsRFC822CA1Cert.crt
  restore_db

  VFY_ACTION="Valid RFC822 nameConstraints Test23"; log_banner
  certImport nameConstraintsRFC822CA2Cert
  crlImport nameConstraintsRFC822CA2CRL.crl
  pkits $certs/ValidRFC822nameConstraintsTest23EE.crt \
      $certs/nameConstraintsRFC822CA2Cert.crt
  restore_db

  VFY_ACTION="Invalid RFC822 nameConstraints Test24"; log_banner
  certImport nameConstraintsRFC822CA2Cert
  crlImport nameConstraintsRFC822CA2CRL.crl
  pkitsn $certs/InvalidRFC822nameConstraintsTest24EE.crt \
      $certs/nameConstraintsRFC822CA2Cert.crt
  restore_db

  VFY_ACTION="Valid RFC822 nameConstraints Test25"; log_banner
  certImport nameConstraintsRFC822CA3Cert
  crlImport nameConstraintsRFC822CA3CRL.crl
  pkits $certs/ValidRFC822nameConstraintsTest25EE.crt \
      $certs/nameConstraintsRFC822CA3Cert.crt
  restore_db

  VFY_ACTION="Invalid RFC822 nameConstraints Test26"; log_banner
  certImport nameConstraintsRFC822CA3Cert
  crlImport nameConstraintsRFC822CA3CRL.crl
  pkitsn $certs/InvalidRFC822nameConstraintsTest26EE.crt \
      $certs/nameConstraintsRFC822CA3Cert.crt
  restore_db

  VFY_ACTION="Valid DN and RFC822 nameConstraints Test27"; log_banner
  certImport nameConstraintsDN1CACert
  crlImport nameConstraintsDN1CACRL.crl
  certImport nameConstraintsDN1subCA3Cert
  crlImport nameConstraintsDN1subCA3CRL.crl
  pkits $certs/ValidDNandRFC822nameConstraintsTest27EE.crt \
      $certs/nameConstraintsDN1subCA3Cert.crt \
      $certs/nameConstraintsDN1CACert.crt
  restore_db

  VFY_ACTION="Invalid DN and RFC822 nameConstraints Test28"; log_banner
  certImport nameConstraintsDN1CACert
  crlImport nameConstraintsDN1CACRL.crl
  certImport nameConstraintsDN1subCA3Cert
  crlImport nameConstraintsDN1subCA3CRL.crl
  pkitsn $certs/InvalidDNandRFC822nameConstraintsTest28EE.crt \
      $certs/nameConstraintsDN1subCA3Cert.crt \
      $certs/nameConstraintsDN1CACert.crt
  restore_db

  VFY_ACTION="Invalid DN and RFC822 nameConstraints Test29"; log_banner
  certImport nameConstraintsDN1CACert
  crlImport nameConstraintsDN1CACRL.crl
  certImport nameConstraintsDN1subCA3Cert
  crlImport nameConstraintsDN1subCA3CRL.crl
  pkitsn $certs/InvalidDNandRFC822nameConstraintsTest29EE.crt \
      $certs/nameConstraintsDN1subCA3Cert.crt \
      $certs/nameConstraintsDN1CACert.crt
  restore_db

  VFY_ACTION="Valid DNS nameConstraints Test30"; log_banner
  certImport nameConstraintsDNS1CACert
  crlImport nameConstraintsDNS1CACRL.crl
  pkits $certs/ValidDNSnameConstraintsTest30EE.crt \
      $certs/nameConstraintsDNS1CACert.crt
  restore_db

  VFY_ACTION="Invalid DNS nameConstraints Test31"; log_banner
  certImport nameConstraintsDNS1CACert
  crlImport nameConstraintsDNS1CACRL.crl
  pkitsn $certs/InvalidDNSnameConstraintsTest31EE.crt \
      $certs/nameConstraintsDNS1CACert.crt
  restore_db

  VFY_ACTION="Valid DNS nameConstraints Test32"; log_banner
  certImport nameConstraintsDNS2CACert
  crlImport nameConstraintsDNS2CACRL.crl
  pkits $certs/ValidDNSnameConstraintsTest32EE.crt \
      $certs/nameConstraintsDNS2CACert.crt
  restore_db

  VFY_ACTION="Invalid DNS nameConstraints Test33"; log_banner
  certImport nameConstraintsDNS2CACert
  crlImport nameConstraintsDNS2CACRL.crl
  pkitsn $certs/InvalidDNSnameConstraintsTest33EE.crt \
      $certs/nameConstraintsDNS2CACert.crt
  restore_db

  VFY_ACTION="Valid URI nameConstraints Test34"; log_banner
  certImport nameConstraintsURI1CACert
  crlImport nameConstraintsURI1CACRL.crl
  pkits $certs/ValidURInameConstraintsTest34EE.crt \
      $certs/nameConstraintsURI1CACert.crt
  restore_db

  VFY_ACTION="Invalid URI nameConstraints Test35"; log_banner
  certImport nameConstraintsURI1CACert
  crlImport nameConstraintsURI1CACRL.crl
  pkitsn $certs/InvalidURInameConstraintsTest35EE.crt \
      $certs/nameConstraintsURI1CACert.crt
  restore_db

  VFY_ACTION="Valid URI nameConstraints Test36"; log_banner
  certImport nameConstraintsURI2CACert
  crlImport nameConstraintsURI2CACRL.crl
  pkits $certs/ValidURInameConstraintsTest36EE.crt \
      $certs/nameConstraintsURI2CACert.crt
  restore_db

  VFY_ACTION="Invalid URI nameConstraints Test37"; log_banner
  certImport nameConstraintsURI2CACert
  crlImport nameConstraintsURI2CACRL.crl
  pkitsn $certs/InvalidURInameConstraintsTest37EE.crt \
      $certs/nameConstraintsURI2CACert.crt
  restore_db

  VFY_ACTION="Invalid DNS nameConstraints Test38"; log_banner
  certImport nameConstraintsDNS1CACert
  crlImport nameConstraintsDNS1CACRL.crl
  pkitsn $certs/InvalidDNSnameConstraintsTest38EE.crt \
      $certs/nameConstraintsDNS1CACert.crt
  restore_db
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
if [ -n "$NSS_PKITS_POLICIES" ]; then
  pkits_CertificatePolicies | tee -a $PKITS_LOG
  pkits_RequireExplicitPolicy | tee -a $PKITS_LOG
  pkits_PolicyMappings | tee -a $PKITS_LOG
  pkits_InhibitPolicyMapping | tee -a $PKITS_LOG
  pkits_InhibitAnyPolicy | tee -a $PKITS_LOG
fi
pkits_NameConstraints | tee -a $PKITS_LOG
pkits_PvtCertExtensions | tee -a $PKITS_LOG
pkits_cleanup

