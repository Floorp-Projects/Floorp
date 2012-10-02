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
"PKCS #12 V2 PBE With SHA-1 and 128 Bit RC4"

  export pkcs12v2pbeWithSha1And40BitRc4=\
"PKCS #12 V2 PBE With SHA-1 and 40 Bit RC4"

  export pkcs12v2pbeWithSha1AndTripleDESCBC=\
"PKCS #12 V2 PBE With SHA-1 and Triple DES-CBC"

  export pkcs12v2pbeWithSha1And128BitRc2Cbc=\
"PKCS #12 V2 PBE With SHA-1 and 128 Bit RC2 CBC"

  export pkcs12v2pbeWithSha1And40BitRc2Cbc=\
"PKCS #12 V2 PBE With SHA-1 and 40 Bit RC2 CBC"

  export pkcs12v2pbeWithMd2AndDESCBC=\
"PKCS #5 Password Based Encryption with MD2 and DES-CBC"

  export pkcs12v2pbeWithMd5AndDESCBC=\
"PKCS #5 Password Based Encryption with MD5 and DES-CBC"

  export pkcs12v2pbeWithSha1AndDESCBC=\
"PKCS #5 Password Based Encryption with SHA-1 and DES-CBC"
  
  export pkcs5pbeWithMD2AndDEScbc=\
"PKCS #5 Password Based Encryption with MD2 and DES-CBC"

  export pkcs5pbeWithMD5AndDEScbc=\
"PKCS #5 Password Based Encryption with MD5 and DES-CBC"

  export pkcs5pbeWithSha1AndDEScbc=\
"PKCS #5 Password Based Encryption with SHA-1 and DES-CBC"

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

  if [ -n "$NSS_ENABLE_ECC" ] ; then
      html_head "Tools Tests with ECC"
  else
      html_head "Tools Tests"
  fi

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
# Export the key and cert to a p12 file using default ciphers
########################################################################
export_with_default_ciphers() 
{
  echo "$SCRIPTNAME: Exporting Alice's key & cert with [default:default] (pk12util -o)"
  echo "pk12util -o Alice.p12 -n \"Alice\" -d ${P_R_ALICEDIR} \\"
  echo "         -k ${R_PWFILE} -w ${R_PWFILE}"
  ${BINDIR}/pk12util -o Alice.p12 -n "Alice" -d ${P_R_ALICEDIR} \
                       -k ${R_PWFILE} -w ${R_PWFILE} 2>&1  
  ret=$?  
  html_msg $ret 0 "Exporting Alices's key & cert with [default:default] (pk12util -o)"
  check_tmpfile
  return $ret
}

########################################################################
# Exports key/cert to a p12 file, the key encryption cipher is specified
# and the cert encryption cipher is blank for default.
########################################################################
export_with_key_cipher() 
{
  # $1 key encryption cipher   
  echo "$SCRIPTNAME: Exporting with [${1}:default]"
  echo "pk12util -o Alice.p12 -n \"Alice\" -d ${P_R_ALICEDIR} \\"
  echo "         -k ${R_PWFILE} -w ${R_PWFILE} -c ${1}"
  ${BINDIR}/pk12util -o Alice.p12 -n "Alice" -d ${P_R_ALICEDIR} \
                     -k ${R_PWFILE} -w ${R_PWFILE} -c "${1}" 2>&1  
  ret=$?  
  html_msg $ret 0 "Exporting with [${1}:default] (pk12util -o)"
  check_tmpfile
  return $ret
}

########################################################################
# Exports key/cert to a p12 file, the key encryption cipher is left
# empty for default and the cert encryption cipher is specified.
########################################################################
export_with_cert_cipher() 
{
  # $1 certificate encryption cipher
  echo "$SCRIPTNAME: Exporting with [default:${1}]"
  echo "pk12util -o Alice.p12 -n \"Alice\" -d ${P_R_ALICEDIR} \\"
  echo "         -k ${R_PWFILE} -w ${R_PWFILE} -C ${1}"
  ${BINDIR}/pk12util -o Alice.p12 -n "Alice" -d ${P_R_ALICEDIR} \
                     -k ${R_PWFILE} -w ${R_PWFILE} -C "${1}" 2>&1  
  ret=$?  
  html_msg $ret 0 "Exporting with [default:${1}] (pk12util -o)"
  check_tmpfile
  return $ret
}

########################################################################
# Exports key/cert to a p12 file, both the key encryption cipher and
# the cert encryption cipher are specified.
########################################################################
export_with_both_key_and_cert_cipher()
{
  # $1 key encryption cipher or ""
  # $2 certificate encryption cipher or ""
  
  echo "pk12util -o Alice.p12 -n \"Alice\" -d ${P_R_ALICEDIR} \\"
  echo "         -k ${R_PWFILE} -w ${R_PWFILE} -c ${1} -C ${2}"     
  ${BINDIR}/pk12util -o Alice.p12 -n Alice -d ${P_R_ALICEDIR} \
                       -k ${R_PWFILE} -w ${R_PWFILE} \
                       -c "${1}" -C "${2}" 2>&1  
  ret=$?    
  html_msg $ret 0 "Exporting with [${1}:${2}] (pk12util -o)"
  check_tmpfile
  return $ret
}

########################################################################
# Exports key and cert to a p12 file, both the key encryption cipher 
# and the cert encryption cipher are specified. The key and cert are
# imported and the p12 file is listed
########################################################################
export_list_import()
{
  # $1 key encryption cipher
  # $2 certificate encryption cipher
    
  if [ "${1}" != "DEFAULT" -a "${2}" != "DEFAULT" ]; then
      export_with_both_key_and_cert_cipher "${1}" "${2}"
  elif [ "${1}" != "DEFAULT" -a "${2}" = "DEFAULT" ]; then
      export_with_key_cipher "${1}"
  elif [ "${1}" = "DEFAULT" -a "${2}" != "DEFAULT" ]; then
      export_with_cert_cipher "${2}"
  else
      export_with_default_ciphers
  fi
    
  list_p12_file Alice.p12
  import_p12_file Alice.p12
}

########################################################################
# Export using the pkcs5pbe ciphers for key and certificate encryption.
# List the contents of and import from the p12 file.
########################################################################
tools_p12_export_list_import_all_pkcs5pbe_ciphers()
{  
  # specify each on key and cert cipher
  for key_cipher in "${pkcs5pbeWithMD2AndDEScbc}" \
                    "${pkcs5pbeWithMD5AndDEScbc}" \
                    "${pkcs5pbeWithSha1AndDEScbc}"\
                    "DEFAULT"; do
      for cert_cipher in "${pkcs5pbeWithMD2AndDEScbc}" \
                         "${pkcs5pbeWithMD5AndDEScbc}" \
                         "${pkcs5pbeWithSha1AndDEScbc}" \
                         "DEFAULT"\
                         "null"; do
            export_list_import "${key_cipher}" "${cert_cipher}"
      done       
  done
}

########################################################################
# Export using the pkcs5v2 ciphers for key and certificate encryption.
# List the contents of and import from the p12 file.
########################################################################
tools_p12_export_list_import_all_pkcs5v2_ciphers()
{
  # These should pass
  for key_cipher in\
    RC2-CBC \
    DES-EDE3-CBC \
    AES-128-CBC \
    AES-192-CBC \
    AES-256-CBC \
    CAMELLIA-128-CBC \
    CAMELLIA-192-CBC \
    CAMELLIA-256-CBC; do

#---------------------------------------------------------------
# Bug 452464 - pk12util -o fails when -C option specifies AES or
# Camellia ciphers
# FIXME Restore these to the list
#    AES-128-CBC, \
#    AES-192-CBC, \
#    AES-256-CBC, \
#    CAMELLIA-128-CBC, \
#    CAMELLIA-192-CBC, \
#    CAMELLIA-256-CBC, \
#  when 452464 is fixed
#---------------------------------------------------------------  
    for cert_cipher in \
      RC2-CBC \
      DES-EDE3-CBC \
      null; do
	  export_list_import ${key_cipher} ${cert_cipher}
	done
  done
}

########################################################################
# Export using the pkcs12v2pbe ciphers for key and certificate encryption.
# List the contents of and import from the p12 file.
########################################################################
tools_p12_export_list_import_all_pkcs12v2pbe_ciphers()
{ 
#---------------------------------------------------------------
# Bug 452471 - pk12util -o fails when -c option specifies pkcs12v2 PBE ciphers
# FIXME - Restore these to the list 
#                "${pkcs12v2pbeWithSha1And128BitRc4}" \
#                "${pkcs12v2pbeWithSha1And40BitRc4}" \
#	             "${pkcs12v2pbeWithSha1AndTripleDESCBC}" \
#                "${pkcs12v2pbeWithSha1And128BitRc2Cbc}" \
#                "${pkcs12v2pbeWithSha1And40BitRc2Cbc}" \
#                "${pkcs12v2pbeWithMd2AndDESCBC}" \
#                "${pkcs12v2pbeWithMd5AndDESCBC}" \
#                "${pkcs12v2pbeWithSha1AndDESCBC}" \
#                "DEFAULT"; do
# when 452471 is fixed
#---------------------------------------------------------------
#  for key_cipher in \
    key_cipher="DEFAULT"
    for cert_cipher in "${pkcs12v2pbeWithSha1And128BitRc4}" \
                  "${pkcs12v2pbeWithSha1And40BitRc4}" \
                  "${pkcs12v2pbeWithSha1AndTripleDESCBC}" \
                  "${pkcs12v2pbeWithSha1And128BitRc2Cbc}" \
                  "${pkcs12v2pbeWithSha1And40BitRc2Cbc}" \
                  "${pkcs12v2pbeWithMd2AndDESCBC}" \
                  "${pkcs12v2pbeWithMd5AndDESCBC}" \
                  "${pkcs12v2pbeWithSha1AndDESCBC}" \
                  "DEFAULT"\
                  "null"; do        
	  export_list_import "${key_cipher}" "${key_cipher}" 
	done
  #done
}

#########################################################################
# Export with no encryption on key should fail but on cert should pass
#########################################################################
tools_p12_export_with_null_ciphers()
{
  # use null as the key encryption algorithm default for the cert one
  # should fail
  
  echo "pk12util -o Alice.p12 -n \"Alice\" -d ${P_R_ALICEDIR} \\"
  echo "         -k ${R_PWFILE} -w ${R_PWFILE} -c null"     
  ${BINDIR}/pk12util -o Alice.p12 -n Alice -d ${P_R_ALICEDIR} \
                       -k ${R_PWFILE} -w ${R_PWFILE} \
                       -c null 2>&1  
  ret=$?
  html_msg $ret 30 "Exporting with [null:default] (pk12util -o)"
  check_tmpfile

  # use default as the key encryption algorithm null for the cert one
  # should pass
  
  echo "pk12util -o Alice.p12 -n \"Alice\" -d ${P_R_ALICEDIR} \\"
  echo "         -k ${R_PWFILE} -w ${R_PWFILE} -C null"     
  ${BINDIR}/pk12util -o Alice.p12 -n Alice -d ${P_R_ALICEDIR} \
                       -k ${R_PWFILE} -w ${R_PWFILE} \
                       -C null 2>&1  
  ret=$?
  html_msg $ret 0 "Exporting with [default:null] (pk12util -o)"
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
  
  export_list_import "DEFAULT" "DEFAULT"

  if [ -n "$NSS_ENABLE_ECC" ] ; then
      echo "$SCRIPTNAME: Exporting Alice's email EC cert & key---------------"
      echo "pk12util -o Alice-ec.p12 -n \"Alice-ec\" -d ${P_R_ALICEDIR} -k ${R_PWFILE} \\"
      echo "         -w ${R_PWFILE}"
      ${BINDIR}/pk12util -o Alice-ec.p12 -n "Alice-ec" -d ${P_R_ALICEDIR} -k ${R_PWFILE} \
           -w ${R_PWFILE} 2>&1 
      ret=$?
      html_msg $ret 0 "Exporting Alice's email EC cert & key (pk12util -o)"
      check_tmpfile

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
  fi
}

############################## tools_p12 ###############################
# local shell function to test basic functionality of pk12util
########################################################################
tools_p12()
{
  tools_p12_export_list_import_with_default_ciphers
  tools_p12_export_list_import_all_pkcs5v2_ciphers
  tools_p12_export_list_import_all_pkcs5pbe_ciphers
  tools_p12_export_list_import_all_pkcs12v2pbe_ciphers
  tools_p12_export_with_null_ciphers
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
tools_cleanup


