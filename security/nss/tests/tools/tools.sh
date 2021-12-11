#! /bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
#
# mozilla/security/nss/tests/tools/tools.sh
#
# Script to test basic functionality of NSS tools
#
# needs to work on all Unix and Windows platforms
#
# tests implemented:
#    pk12util
#    signtool
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
########################################################################

  export pkcs12v2pbeWithSha1And128BitRc4=\
"PKCS #12 V2 PBE With SHA-1 And 128 Bit RC4"

  export pkcs12v2pbeWithSha1And40BitRc4=\
"PKCS #12 V2 PBE With SHA-1 And 40 Bit RC4"

  export pkcs12v2pbeWithSha1AndTripleDESCBC=\
"PKCS #12 V2 PBE With SHA-1 And 3KEY Triple DES-CBC"

  export pkcs12v2pbeWithSha1And128BitRc2Cbc=\
"PKCS #12 V2 PBE With SHA-1 And 128 Bit RC2 CBC"

  export pkcs12v2pbeWithSha1And40BitRc2Cbc=\
"PKCS #12 V2 PBE With SHA-1 And 40 Bit RC2 CBC"

  export pkcs5pbeWithMD2AndDEScbc=\
"PKCS #5 Password Based Encryption with MD2 and DES-CBC"

  export pkcs5pbeWithMD5AndDEScbc=\
"PKCS #5 Password Based Encryption with MD5 and DES-CBC"

  export pkcs5pbeWithSha1AndDEScbc=\
"PKCS #5 Password Based Encryption with SHA-1 and DES-CBC"

  # if we change the defaults in pk12util, update these variables
  export CERT_ENCRYPTION_DEFAULT="AES-128-CBC"
  export KEY_ENCRYPTION_DEFAULT="AES-256-CBC"
  export HASH_DEFAULT="SHA-256"

  export PKCS5v1_PBE_CIPHERS="${pkcs5pbeWithMD2AndDEScbc},\
${pkcs5pbeWithMD5AndDEScbc},\
${pkcs5pbeWithSha1AndDEScbc}"
  export PKCS12_PBE_CIPHERS="${pkcs12v2pbeWithSha1And128BitRc4},\
${pkcs12v2pbeWithSha1And40BitRc4},\
${pkcs12v2pbeWithSha1AndTripleDESCBC},\
${pkcs12v2pbeWithSha1And128BitRc2Cbc},\
${pkcs12v2pbeWithSha1And40BitRc2Cbc}"
  export PKCS5v2_PBE_CIPHERS="RC2-CBC,DES-EDE3-CBC,AES-128-CBC,AES-192-CBC,\
AES-256-CBC,CAMELLIA-128-CBC,CAMELLIA-192-CBC,CAMELLIA-256-CBC"
  export PBE_CIPHERS="${PKCS5v1_PBE_CIPHERS},${PKCS12_PBE_CIPHERS},${PKCS5v2_PBE_CIPHERS}"
  export PBE_CIPHERS_CLASSES="${pkcs5pbeWithSha1AndDEScbc},\
${pkcs12v2pbeWithSha1AndTripleDESCBC},AES-256-CBC,default"
  export PBE_HASH="SHA-1,SHA-224,SHA-256,SHA-384,SHA-512,default"

############################## tools_init ##############################
# local shell function to initialize this script
########################################################################
tools_init()
{
  SCRIPTNAME=tools.sh      # sourced - $0 would point to all.sh

  if [ -z "${CLEANUP}" ] ; then     # if nobody else is responsible for
      CLEANUP="${SCRIPTNAME}"       # cleaning this script will do it
  fi

  if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ]; then
      cd ../common
      . ./init.sh
  fi
  if [ ! -r $CERT_LOG_FILE ]; then  # we need certificates here
      cd ../cert
      . ./cert.sh
  fi
  SCRIPTNAME=tools.sh

  html_head "Tools Tests"

  grep "SUCCESS: SMIME passed" $CERT_LOG_FILE >/dev/null || {
      Exit 15 "Fatal - S/MIME of cert.sh needs to pass first"
  }

  TOOLSDIR=${HOSTDIR}/tools
  COPYDIR=${TOOLSDIR}/copydir
  SIGNDIR=${TOOLSDIR}/signdir

  R_TOOLSDIR=../tools
  R_COPYDIR=../tools/copydir
  R_SIGNDIR=../tools/signdir
  P_R_COPYDIR=${R_COPYDIR}
  P_R_SIGNDIR=${R_SIGNDIR}
  if [ -n "${MULTIACCESS_DBM}" ]; then
      P_R_COPYDIR="multiaccess:Tools.$version"
      P_R_SIGNDIR="multiaccess:Tools.sign.$version"
  fi

  mkdir -p ${TOOLSDIR}
  mkdir -p ${COPYDIR}
  mkdir -p ${SIGNDIR}
  cp ${ALICEDIR}/* ${SIGNDIR}/
  mkdir -p ${TOOLSDIR}/html
  cp ${QADIR}/tools/sign*.html ${TOOLSDIR}/html
  mkdir -p ${TOOLSDIR}/data
  cp ${QADIR}/tools/TestOldCA.p12 ${TOOLSDIR}/data
  cp ${QADIR}/tools/TestOldAES128CA.p12 ${TOOLSDIR}/data
  cp ${QADIR}/tools/TestRSAPSS.p12 ${TOOLSDIR}/data

  cd ${TOOLSDIR}
}

########################## list_p12_file ###############################
# List the key and cert in the specified p12 file
########################################################################
list_p12_file()
{
  echo "$SCRIPTNAME: Listing Alice's pk12 file"
  echo "pk12util -l ${1} -w ${R_PWFILE}"

  ${BINDIR}/pk12util -l ${1} -w ${R_PWFILE} 2>&1
  ret=$?
  html_msg $ret 0 "Listing ${1} (pk12util -l)"
  check_tmpfile
}

########################################################################
# Import the key and cert from the specified p12 file
########################################################################
import_p12_file()
{
  echo "$SCRIPTNAME: Importing Alice's pk12 ${1} file"
  echo "pk12util -i ${1} -d ${P_R_COPYDIR} -k ${R_PWFILE} -w ${R_PWFILE}"

  ${BINDIR}/pk12util -i ${1} -d ${P_R_COPYDIR} -k ${R_PWFILE} -w ${R_PWFILE} 2>&1
  ret=$?
  html_msg $ret 0 "Importing ${1} (pk12util -i)"
  check_tmpfile
}


########################################################################
# Export the key and cert from the specified p12 file
########################################################################
export_p12_file()
{
  # $1 p12 file
  # $2 cert to export
  # $3 certdb
  # $4 key encryption cipher or "default"
  # $5 certificate encryption cipher or "default"
  # $6 hash algorithm or "default"
  KEY_CIPHER_OPT="-c"
  KEY_CIPHER="${4}"
  CERT_CIPHER_OPT="-C"
  CERT_CIPHER="${5}"
  HASH_ALG_OPT="-M"
  HASH_ALG="${6}"

  if [ "${KEY_CIPHER}" = "default" ]; then
    KEY_CIPHER_OPT=""
    KEY_CIPHER=""
  fi
  if [ "${CERT_CIPHER}" = "default" ]; then
    CERT_CIPHER_OPT=""
    CERT_CIPHER=""
  fi
  if [ "${HASH_ALG}" = "default" ]; then
    HASH_ALG_OPT=""
    HASH_ALG=""
  fi

  echo "pk12util -o \"${1}\" -n \"${2}\" -d \"${3}\" \\"
  echo "         -k ${R_PWFILE} -w ${R_PWFILE} \\"
  echo "         ${KEY_CIPHER_OPT} \"${KEY_CIPHER}\" \\"
  echo "         ${CERT_CIPHER_OPT} \"${CERT_CIPHER}\" \\"
  echo "         ${HASH_ALG_OPT} \"${HASH_ALG}\""
  ${BINDIR}/pk12util -o "${1}" -n "${2}" -d "${3}" \
                       -k ${R_PWFILE} -w ${R_PWFILE} \
                       ${KEY_CIPHER_OPT} "${KEY_CIPHER}" \
                       ${CERT_CIPHER_OPT} "${CERT_CIPHER}" \
                       ${HASH_ALG_OPT} "${HASH_ALG}" 2>&1
  ret=$?
  html_msg $ret 0 "Exporting with [${4}:${5}:${6}] (pk12util -o)"
  check_tmpfile
  verify_p12 "${1}" "${4}" "${5}" "${6}"
  return $ret
}

########################################################################
# Exports key and cert to a p12 file, the key encryption cipher,
# the cert encryption cipher, and/or the hash algorithm are specified.
# The key and cert are imported and the p12 file is listed
########################################################################
export_list_import()
{
  export_p12_file Alice.p12 Alice "${P_R_ALICEDIR}" "${@}"
  list_p12_file Alice.p12
  import_p12_file Alice.p12
}

########################################################################
# Export using the pkcs5pbe ciphers for key and certificate encryption.
# List the contents of and import from the p12 file.
########################################################################
tools_p12_export_list_import_all_pkcs5pbe_ciphers()
{
  local saveIFS="${IFS}"
  IFS=,
  for key_cipher in ${PKCS5v1_PBE_CIPHERS} default; do
      for cert_cipher in ${PKCS5v1_PBE_CIPHERS} default none; do
          for hash in ${PBE_HASH}; do
                  export_list_import "${key_cipher}" "${cert_cipher}" "${hash}"
           done
      done
  done
  IFS="${saveIFS}"
}

########################################################################
# Export using the pkcs5v2 ciphers for key and certificate encryption.
# List the contents of and import from the p12 file.
########################################################################
tools_p12_export_list_import_all_pkcs5v2_ciphers()
{
  local saveIFS="${IFS}"
  IFS=,
  for key_cipher in ${PKCS5v2_PBE_CIPHERS} default; do
      for cert_cipher in ${PKCS5v2_PBE_CIPHERS} default none; do
          for hash in ${PBE_HASH}; do
                  export_list_import "${key_cipher}" "${cert_cipher}" "${hash}"
           done
      done
  done
  IFS="${saveIFS}"
}

########################################################################
# Export using the pkcs12v2pbe ciphers for key and certificate encryption.
# List the contents of and import from the p12 file.
########################################################################
tools_p12_export_list_import_all_pkcs12v2pbe_ciphers()
{
  local saveIFS="${IFS}"
  IFS=,
  for key_cipher in ${PKCS12_PBE_CIPHERS} ${PKCS5v1_PBE_CIPHERS} default; do
      for cert_cipher in ${PKCS12_PBE_CIPHERS} ${PKCS5v1_PBE_CIPHERS} default none; do
          for hash in ${PBE_HASH}; do
                  export_list_import "${key_cipher}" "${cert_cipher}" "${hash}"
           done
      done
  done
  IFS="${saveIFS}"
}

########################################################################
# Spot check all ciphers.
# using the traditional tests, we wind up running almost 1300 tests.
# This isn't too bad for debug builds in which the interator is set to 1000.
# for optimized builds, the iterator is set to 60000, which means a 30
# minute test will  now take more than 2 hours. This tests most combinations
# and results in only about 300 tests. We are stil testing all ciphers
# for both key and cert encryption, and we are testing them against
# one of each class of cipher (pkcs5v1, pkcs5v2, pkcs12).
########################################################################
tools_p12_export_list_import_most_ciphers()
{
  local saveIFS="${IFS}"
  IFS=,
  for cipher in ${PBE_CIPHERS}; do
    for class in ${PBE_CIPHERS_CLASSES}; do
      # we'll test the case of cipher == class below the for loop
      if [ "${cipher}" != "${class}" ]; then
          export_list_import "${class}" "${cipher}" "SHA-1"
          export_list_import "${cipher}" "${class}" "SHA-256"
      fi
    done
    export_list_import "${cipher}" "none" "SHA-224"
    export_list_import "${cipher}" "${cipher}" "SHA-384"
  done
  for class in ${PBE_CIPHERS_CLASSES}; do
    for hash in ${PBE_HASH}; do
      export_list_import "${class}" "${class}" "${hash}"
    done
  done
  IFS="${saveIFS}"
}

#########################################################################
# Export with no encryption on key should fail but on cert should pass
#########################################################################
tools_p12_export_with_none_ciphers()
{
  # use none as the key encryption algorithm default for the cert one
  # should fail

  echo "pk12util -o Alice.p12 -n \"Alice\" -d ${P_R_ALICEDIR} \\"
  echo "         -k ${R_PWFILE} -w ${R_PWFILE} -c none"
  ${BINDIR}/pk12util -o Alice.p12 -n Alice -d ${P_R_ALICEDIR} \
                       -k ${R_PWFILE} -w ${R_PWFILE} \
                       -c none 2>&1
  ret=$?
  html_msg $ret 30 "Exporting with [none:default:default] (pk12util -o)"
  check_tmpfile

  # use default as the key encryption algorithm none for the cert one
  # should pass

  echo "pk12util -o Alice.p12 -n \"Alice\" -d ${P_R_ALICEDIR} \\"
  echo "         -k ${R_PWFILE} -w ${R_PWFILE} -C none"
  ${BINDIR}/pk12util -o Alice.p12 -n Alice -d ${P_R_ALICEDIR} \
                       -k ${R_PWFILE} -w ${R_PWFILE} \
                       -C none 2>&1
  ret=$?
  html_msg $ret 0 "Exporting with [default:none:default] (pk12util -o)"
  check_tmpfile
  verify_p12 Alice.p12 "default" "none" "default"
}

#########################################################################
# Export with invalid cipher should fail
#########################################################################
tools_p12_export_with_invalid_ciphers()
{
  echo "pk12util -o Alice.p12 -n \"Alice\" -d ${P_R_ALICEDIR} \\"
  echo "         -k ${R_PWFILE} -w ${R_PWFILE} -c INVALID_CIPHER"
  ${BINDIR}/pk12util -o Alice.p12 -n Alice -d ${P_R_ALICEDIR} \
                       -k ${R_PWFILE} -w ${R_PWFILE} \
                       -c INVALID_CIPHER 2>&1
  ret=$?
  html_msg $ret 30 "Exporting with [INVALID_CIPHER:default] (pk12util -o)"
  check_tmpfile

  echo "pk12util -o Alice.p12 -n \"Alice\" -d ${P_R_ALICEDIR} \\"
  echo "         -k ${R_PWFILE} -w ${R_PWFILE} -C INVALID_CIPHER"
  ${BINDIR}/pk12util -o Alice.p12 -n Alice -d ${P_R_ALICEDIR} \
                       -k ${R_PWFILE} -w ${R_PWFILE} \
                       -C INVALID_CIPHER 2>&1
  ret=$?
  html_msg $ret 30 "Exporting with [default:INVALID_CIPHER] (pk12util -o)"
  check_tmpfile

}

#########################################################################
# Exports using the default key and certificate encryption ciphers.
# Imports from  and lists the contents of the p12 file.
# Repeats the test with ECC if enabled.
########################################################################
tools_p12_export_list_import_with_default_ciphers()
{
  echo "$SCRIPTNAME: Exporting Alice's email cert & key - default ciphers"

  export_list_import "default" "default" "default"

  echo "$SCRIPTNAME: Exporting Alice's email EC cert & key---------------"
  echo "pk12util -o Alice-ec.p12 -n \"Alice-ec\" -d ${P_R_ALICEDIR} -k ${R_PWFILE} \\"
  echo "         -w ${R_PWFILE}"
  ${BINDIR}/pk12util -o Alice-ec.p12 -n "Alice-ec" -d ${P_R_ALICEDIR} -k ${R_PWFILE} \
       -w ${R_PWFILE} 2>&1
  ret=$?
  html_msg $ret 0 "Exporting Alice's email EC cert & key (pk12util -o)"
  check_tmpfile
  verify_p12 Alice-ec.p12 "default" "default" "default"

  echo "$SCRIPTNAME: Importing Alice's email EC cert & key --------------"
  echo "pk12util -i Alice-ec.p12 -d ${P_R_COPYDIR} -k ${R_PWFILE} -w ${R_PWFILE}"
  ${BINDIR}/pk12util -i Alice-ec.p12 -d ${P_R_COPYDIR} -k ${R_PWFILE} -w ${R_PWFILE} 2>&1
  ret=$?
  html_msg $ret 0 "Importing Alice's email EC cert & key (pk12util -i)"
  check_tmpfile

  echo "$SCRIPTNAME: Listing Alice's pk12 EC file -----------------"
  echo "pk12util -l Alice-ec.p12 -w ${R_PWFILE}"
  ${BINDIR}/pk12util -l Alice-ec.p12 -w ${R_PWFILE} 2>&1
  ret=$?
  html_msg $ret 0 "Listing Alice's pk12 EC file (pk12util -l)"
  check_tmpfile
}

tools_p12_import_old_files()
{
  echo "$SCRIPTNAME: Importing PKCS#12 files created with older NSS --------------"
  echo "pk12util -i TestOldCA.p12 -d ${P_R_COPYDIR} -k ${R_PWFILE} -w ${R_PWFILE}"
  ${BINDIR}/pk12util -i ${TOOLSDIR}/data/TestOldCA.p12 -d ${P_R_COPYDIR} -k ${R_PWFILE} -w ${R_PWFILE} 2>&1
  ret=$?
  html_msg $ret 0 "Importing PKCS#12 file created with NSS 3.21 (PBES2 with BMPString password)"
  check_tmpfile

  echo "pk12util -i TestOldAES128CA.p12 -d ${P_R_COPYDIR} -k ${R_PWFILE} -w ${R_PWFILE}"
  ${BINDIR}/pk12util -i ${TOOLSDIR}/data/TestOldAES128CA.p12 -d ${P_R_COPYDIR} -k ${R_PWFILE} -w ${R_PWFILE} 2>&1
  ret=$?
  html_msg $ret 0 "Importing PKCS#12 file created with NSS 3.29.5 (PBES2 with incorrect AES-128-CBC algorithm ID)"
  check_tmpfile
}

tools_p12_import_rsa_pss_private_key()
{
  echo "$SCRIPTNAME: Importing RSA-PSS private key from PKCS#12 file --------------"
  ${BINDIR}/pk12util -i ${TOOLSDIR}/data/TestRSAPSS.p12 -d ${P_R_COPYDIR} -k ${R_PWFILE} -W '' 2>&1
  ret=$?
  html_msg $ret 0 "Importing RSA-PSS private key from PKCS#12 file"
  check_tmpfile

  # Check if RSA-PSS identifier is included in the key listing
  ${BINDIR}/certutil -d ${P_R_COPYDIR} -K -f ${R_PWFILE} | grep '^<[0-9 ]*> *rsaPss'
  ret=$?
  html_msg $ret 0 "Listing RSA-PSS private key imported from PKCS#12 file"
  check_tmpfile

  return $ret
}

############################## tools_p12 ###############################
# local shell function to test basic functionality of pk12util
########################################################################
tools_p12()
{
  tools_p12_export_list_import_with_default_ciphers
  # optimized builds have a larger iterator, so they can't run as many
  # pkcs12 tests and complete in a reasonable time. Use the iterateration
  # count from the previous tests to determine how many tests
  # we can run.
  iteration_count=$(pp -t p12 -i Alice-ec.p12 | grep "Iterations: " | sed -e 's;.*Iterations: ;;' -e 's;(.*).*;;')
  echo "Iteration count=${iteration_count}"
  if [ -n "${iteration_count}" -a  ${iteration_count} -le 10000 ]; then
      tools_p12_export_list_import_all_pkcs5v2_ciphers
      tools_p12_export_list_import_all_pkcs12v2pbe_ciphers
  else
      tools_p12_export_list_import_most_ciphers
  fi
  tools_p12_export_with_none_ciphers
  tools_p12_export_with_invalid_ciphers
  tools_p12_import_old_files
  if [ "${TEST_MODE}" = "SHARED_DB" ] ; then
    tools_p12_import_rsa_pss_private_key
  fi
}

############################## tools_sign ##############################
# local shell function pk12util uses a hardcoded tmp file, if this exists
# and is owned by another user we don't get reasonable errormessages
########################################################################
check_tmpfile()
{
  if [ $ret != "0" -a -f /tmp/Pk12uTemp ] ; then
      echo "Error: pk12util temp file exists. Please remove this file and"
      echo "       rerun the test (/tmp/Pk12uTemp) "
  fi
}

############################## tools_sign ##############################
# make sure the generated p12 file has the characteristics we expected
########################################################################
verify_p12()
{
  KEY_ENCRYPTION=$(map_cipher "${2}" "${KEY_ENCRYPTION_DEFAULT}")
  CERT_ENCRYPTION=$(map_cipher "${3}" "${CERT_ENCRYPTION_DEFAULT}")
  HASH=$(map_cipher "${4}" "${HASH_DEFAULT}")

  STATE="NOBAGS"   # state records if we are in the key or cert bag
  CERT_ENCRYPTION_NOT_FOUND=1
  KEY_ENCRYPTION_NOT_FOUND=1
  CERT_ENCRYPTION_FAIL=0
  KEY_ENCRYPTION_FAIL=0
  HASH_FAIL=0
  TMP=$(mktemp /tmp/p12Verify.XXXXXX)
  which pk12util
  local saveIFS="${IFS}"
  IFS=" 	\
"
  # use pp to dump the pkcs12 file, only the unencrypted portions are visible
  # if there are multiple entries, we fail if any of those entries have the
  # wrong encryption. We also fail if we can't find any encryption info.
  # Use a file rather than a pipe so that while do can modify our variables.
  # We're only interested in extracting the encryption algorithms are here,
  # p12util -l will verify that decryption works properly.
  pp -t pkcs12 -i ${1} -o ${TMP}
  while read line ; do
     # first up: if we see an unencrypted key bag, then we know that the key
     # was unencrypted (NOTE: pk12util currently can't generate these kinds of
     # files).
     if [[ "${line}" =~ "Bag "[0-9]+" ID: PKCS #12 V1 Key Bag" ]]; then
        KEY_ENCRYPTION_NOT_FOUND=0
        if [ "${KEY_ENCRYPTION}" != "none" ]; then
            KEY_ENCRYPTION_FAIL=1
            echo "--Key encryption mismatch: expected \"${KEY_ENCRYPTION}\" found \"none\""
        fi
       continue
     fi
     # if we find the the Cert Bag, then we know that the certificate was not
     # encrypted
     if [[ "${line}" =~ "Bag "[0-9]+" ID: PKCS #12 V1 Cert Bag" ]]; then
        CERT_ENCRYPTION_NOT_FOUND=0
        if [ "${CERT_ENCRYPTION}" != "none" ]; then
            CERT_ENCRYPTION_FAIL=1
           echo "--Cert encryption mismatch: expected \"${CERT_ENCRYPTION}\" found \"none\""
        fi
        continue
     fi
     # we found the shrouded key bag, the next encryption informtion should be
     # for the key.
     if [[ "${line}" =~ "Bag "[0-9]+" ID: PKCS #12 V1 PKCS8 Shrouded Key Bag" ]]; then
        STATE="KEY"
        continue
     fi
     # If we found PKCS #7 Encrypted Data, it must be the encrypted certificate
     # (well it could be any encrypted certificate, or a crl, but in p12util
     # they will all have the same encryption value
     if [[ "${line}" =  "PKCS #7 Encrypted Data:" ]]; then
        STATE="CERT"
        continue
     fi
     # check the Mac
     if [[ "${line}" =~ "Mac Digest Algorithm ID: ".* ]]; then
        MAC="${line##Mac Digest Algorithm ID: }"
        if [ "${MAC}" != "${HASH}" ]; then
            HASH_FAIL=1
            echo "--Mac Hash mismatch: expected \"${HASH}\" found \"${MAC}\""
        fi
     fi
     # check the KDF
     if [[ "${line}" =~ "KDF algorithm: ".* ]]; then
        KDF="${line##KDF algorithm: }"
        if [ "${KDF}" != "HMAC ${HASH}" ]; then
            HASH_FAIL=1
            echo "--KDF Hash mismatch: expected \"HMAC ${HASH}\" found \"${KDF}\""
        fi
     fi
     # Content Encryption Algorithm is the PKCS #5 algorithm ID.
     if [[  "${line}" =~ .*"Encryption Algorithm: ".* ]]; then
        # Strip the [Content ]EncryptionAlgorithm
        ENCRYPTION="${line##Content }"
        ENCRYPTION="${ENCRYPTION##Encryption Algorithm: }"
        # If that algorithm id is PKCS #5 V2, then skip forward looking
        # for the Cipher: field.
        if [[ "${ENCRYPTION}" =~ "PKCS #5 Password Based Encryption v2"\ * ]]; then
            continue;
        fi
        case ${STATE} in
        "KEY")
            KEY_ENCRYPTION_NOT_FOUND=0
            if [ "${KEY_ENCRYPTION}" != "${ENCRYPTION}" ]; then
                KEY_ENCRYPTION_FAIL=1
                echo "--Key encryption mismatch: expected \"${KEY_ENCRYPTION}\" found \"${ENCRYPTION}\""
            fi
            ;;
        "CERT")
            CERT_ENCRYPTION_NOT_FOUND=0
            if [ "${CERT_ENCRYPTION}" != "${ENCRYPTION}" ]; then
                CERT_ENCRYPTION_FAIL=1
                echo "--Cert encryption mismatch: expected \"${CERT_ENCRYPTION}\" found \"${ENCRYPTION}\""
            fi
            ;;
        esac
     fi
     # handle the PKCS 5 case
     if [[ "${line}" =~ "Cipher: ".* ]]; then
        ENCRYPTION="${line#Cipher: }"
        case ${STATE} in
        "KEY")
            KEY_ENCRYPTION_NOT_FOUND=0
            if [ "${KEY_ENCRYPTION}" != "${ENCRYPTION}" ]; then
                KEY_ENCRYPTION_FAIL=1
                echo "--Key encryption mismatch: expected \"${KEY_ENCRYPTION}\" found \"${ENCRYPTION}\""
            fi
            ;;
        "CERT")
            CERT_ENCRYPTION_NOT_FOUND=0
            if [ "${CERT_ENCRYPTION}" != "${ENCRYPTION}" ]; then
                CERT_ENCRYPTION_FAIL=1
                echo "--Cert encryption mismatch: expected \"${CERT_ENCRYPTION}\" found \"${ENCRYPTION}\""
            fi
            ;;
        esac
     fi
  done < ${TMP}
  IFS="${saveIFS}"
  # we've scanned the file, set the return value to a combination of
  # KEY and CERT state variables. If everything is as expected, they should
  # add up to 0.
  ret=$((${HASH_FAIL} * 10000 + ${KEY_ENCRYPTION_FAIL} * 1000 + ${KEY_ENCRYPTION_NOT_FOUND} * 100 + ${CERT_ENCRYPTION_FAIL} * 10 + ${CERT_ENCRYPTION_NOT_FOUND}))
  rm -r ${TMP}
  html_msg $ret 0 "Verifying p12 file generated with [${2}:${3}:${4}]"
}

#
# this handles any mapping we need from requested cipher to
# actual cipher. For instance ciphers which already have
# PKCS 5 v1 PBE will be mapped to those pbes by pk12util.
map_cipher()
{
   if [ "${1}" = "default" ]; then
      echo "${2}"
      return
   fi
   case "${1}" in
   # these get mapped to the PKCS5 v1 or PKCS 12 attributes, not PKCS 5v2
   RC2-CBC)
      echo "${pkcs12v2pbeWithSha1And128BitRc2Cbc}"
      return ;;
   DES-EDE3-CBC)
      echo "${pkcs12v2pbeWithSha1AndTripleDESCBC}"
      return;;
   esac
   echo "${1}"
}

############################## tools_sign ##############################
# local shell function to test basic functionality of signtool
########################################################################
tools_sign()
{
  echo "$SCRIPTNAME: Create objsign cert -------------------------------"
  echo "signtool -G \"objectsigner\" -d ${P_R_SIGNDIR} -p \"nss\""
  ${BINDIR}/signtool -G "objsigner" -d ${P_R_SIGNDIR} -p "nss" 2>&1 <<SIGNSCRIPT
y
TEST
MOZ
NSS
NY
US
liz
liz@moz.org
SIGNSCRIPT
  html_msg $? 0 "Create objsign cert (signtool -G)"

  echo "$SCRIPTNAME: Signing a jar of files ----------------------------"
  echo "signtool -Z nojs.jar -d ${P_R_SIGNDIR} -p \"nss\" -k objsigner \\"
  echo "         ${R_TOOLSDIR}/html"
  ${BINDIR}/signtool -Z nojs.jar -d ${P_R_SIGNDIR} -p "nss" -k objsigner \
           ${R_TOOLSDIR}/html
  html_msg $? 0 "Signing a jar of files (signtool -Z)"

  echo "$SCRIPTNAME: Listing signed files in jar ----------------------"
  echo "signtool -v nojs.jar -d ${P_R_SIGNDIR} -p nss -k objsigner"
  ${BINDIR}/signtool -v nojs.jar -d ${P_R_SIGNDIR} -p nss -k objsigner
  html_msg $? 0 "Listing signed files in jar (signtool -v)"

  echo "$SCRIPTNAME: Show who signed jar ------------------------------"
  echo "signtool -w nojs.jar -d ${P_R_SIGNDIR}"
  ${BINDIR}/signtool -w nojs.jar -d ${P_R_SIGNDIR}
  html_msg $? 0 "Show who signed jar (signtool -w)"

  echo "$SCRIPTNAME: Signing a xpi of files ----------------------------"
  echo "signtool -Z nojs.xpi -X -d ${P_R_SIGNDIR} -p \"nss\" -k objsigner \\"
  echo "         ${R_TOOLSDIR}/html"
  ${BINDIR}/signtool -Z nojs.xpi -X -d ${P_R_SIGNDIR} -p "nss" -k objsigner \
           ${R_TOOLSDIR}/html
  html_msg $? 0 "Signing a xpi of files (signtool -Z -X)"

  echo "$SCRIPTNAME: Listing signed files in xpi ----------------------"
  echo "signtool -v nojs.xpi -d ${P_R_SIGNDIR} -p nss -k objsigner"
  ${BINDIR}/signtool -v nojs.xpi -d ${P_R_SIGNDIR} -p nss -k objsigner
  html_msg $? 0 "Listing signed files in xpi (signtool -v)"

  echo "$SCRIPTNAME: Show who signed xpi ------------------------------"
  echo "signtool -w nojs.xpi -d ${P_R_SIGNDIR}"
  ${BINDIR}/signtool -w nojs.xpi -d ${P_R_SIGNDIR}
  html_msg $? 0 "Show who signed xpi (signtool -w)"

}

tools_modutil()
{
  echo "$SCRIPTNAME: Test if DB created by modutil -create is initialized"
  mkdir -p ${R_TOOLSDIR}/moddir
  # copied from modu function in cert.sh
  # echo is used to press Enter expected by modutil
  echo | ${BINDIR}/modutil -create -dbdir "${R_TOOLSDIR}/moddir" 2>&1
  ret=$?
  ${BINDIR}/certutil -S -s 'CN=TestUser' -d "${TOOLSDIR}/moddir" -n TestUser \
	   -x -t ',,' -z "${R_NOISE_FILE}"
  ret=$?
  html_msg $ret 0 "Test if DB created by modutil -create is initialized"
  check_tmpfile
}

############################## tools_cleanup ###########################
# local shell function to finish this script (no exit since it might be
# sourced)
########################################################################
tools_cleanup()
{
  html "</TABLE><BR>"
  cd ${QADIR}
  . common/cleanup.sh
}

################## main #################################################

tools_init
tools_p12
tools_sign
tools_modutil
tools_cleanup


