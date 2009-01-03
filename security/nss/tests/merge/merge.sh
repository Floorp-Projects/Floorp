#! /bin/sh  
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
#   Dr Vipul Gupta <vipul.gupta@sun.com>, Sun Microsystems Laboratories
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

########################################################################
#
# mozilla/security/nss/tests/merge/merge.sh
#
# Script to test NSS merge
#
# needs to work on all Unix and Windows platforms
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
########################################################################

############################## merge_init ##############################
# local shell function to initialize this script
########################################################################
merge_init()
{
  SCRIPTNAME=merge.sh      # sourced - $0 would point to all.sh
  HAS_EXPLICIT_DB=0
  if [ ! -z "${NSS_DEFAULT_DB_TYPE}" ]; then
     HAS_EXPLICIT_DB=1
  fi


  if [ -z "${CLEANUP}" ] ; then     # if nobody else is responsible for
      CLEANUP="${SCRIPTNAME}"       # cleaning this script will do it
  fi

  if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ]; then
      cd ../common
      . ./init.sh
  fi
  if [ ! -r $CERT_LOG_FILE ]; then  # we need certificates here
      cd ${QADIR}/cert
      . ./cert.sh
  fi

  if [ ! -d ${HOSTDIR}/SDR ]; then
      cd ${QADIR}/sdr
      . ./sdr.sh
  fi
  SCRIPTNAME=merge.sh

  html_head "Merge Tests"

  # need the SSL & SMIME directories from cert.sh
  grep "SUCCESS: SMIME passed" $CERT_LOG_FILE >/dev/null || {
      Exit 11 "Fatal - S/MIME of cert.sh needs to pass first"
  }
  grep "SUCCESS: SSL passed" $CERT_LOG_FILE >/dev/null || {
      Exit 8 "Fatal - SSL of cert.sh needs to pass first"
  }

  #temporary files for SDR tests
  VALUE1=$HOSTDIR/tests.v1.$$
  VALUE3=$HOSTDIR/tests.v3.$$

  # local directories used in this test.
  MERGEDIR=${HOSTDIR}/merge
  R_MERGEDIR=../merge
  D_MERGE="merge.$version"
  # SDR not initialized in common/init
  P_R_SDR=../SDR
  D_SDR="SDR.$version"
  mkdir -p ${MERGEDIR}

  PROFILE=.
  if [ -n "${MULTIACCESS_DBM}" ]; then
     PROFILE="multiaccess:${D_MERGE}"
     P_R_SDR="multiaccess:${D_SDR}"
  fi

  cd ${MERGEDIR}

  # clear out any existing databases, potentially from a previous run.
  rm -f *.db

  # copy alicedir over as a seed database.
  cp ${R_ALICEDIR}/* .
  # copy the smime text samples
  cp ${QADIR}/smime/*.txt .

  #
  # allow all the tests to run in standalone mode.
  #  in standalone mode, TEST_MODE is not set.
  #  if NSS_DEFAULT_DB_TYPE is dbm, then test merge with dbm
  #  if NSS_DEFAULT_DB_TYPE is sql, then test merge with sql
  #  if NSS_DEFAULT_DB_TYPE is not set, then test database upgrade merge
  #   from dbm databases (created above) into a new sql db.
  if [ -z "${TEST_MODE}" ] && [ ${HAS_EXPLICIT_DB} -eq 0 ]; then
	echo "*** Using Standalone Upgrade DB mode"
	export NSS_DEFAULT_DB_TYPE=sql
	echo certutil --upgrade-merge --source-dir ${P_R_ALICEDIR} --upgrade-id local -d ${PROFILE} -f ${R_PWFILE} -@ ${R_PWFILE}
	${BINDIR}/certutil --upgrade-merge --source-dir ${P_R_ALICEDIR} --upgrade-id local -d ${PROFILE}  -f ${R_PWFILE} -@ ${R_PWFILE}
	TEST_MODE=UPGRADE_DB

  fi
	
}

#
# this allows us to run this test for both merge and upgrade-merge cases.
# merge_cmd takes the potential upgrade-id and the rest of the certutil
# arguments.
#
merge_cmd()
{
  MERGE_CMD=--merge
  if [ "${TEST_MODE}" = "UPGRADE_DB" ]; then
     MERGE_CMD="--upgrade-merge --upgrade-token-name OldDB --upgrade-id ${1}"
  fi
  shift
  echo certutil ${MERGE_CMD} $*
  ${PROFTOOL} ${BINDIR}/certutil ${MERGE_CMD} $*
}


merge_main()
{
  # first create a local sdr key and encrypt some data with it
  # This will cause a colision with the SDR key in ../SDR.
  echo "$SCRIPTNAME: Creating an SDR key & Encrypt"
  echo "sdrtest -d ${PROFILE} -o ${VALUE3} -t Test2 -f ${R_PWFILE}"
  ${PROFTOOL} ${BINDIR}/sdrtest -d ${PROFILE} -o ${VALUE3} -t Test2 -f ${R_PWFILE}
  html_msg $? 0 "Creating SDR Key"

  # Now merge in Dave
  # Dave's cert is already in alicedir, but his key isn't. This will make
  # sure we are updating the keys and CKA_ID's on the certificate properly.
  MERGE_ID=dave
  echo "$SCRIPTNAME: Merging in Key for Existing user"
  merge_cmd dave --source-dir ${P_R_DAVEDIR} -d ${PROFILE} -f ${R_PWFILE} -@ ${R_PWFILE}
  html_msg $? 0 "Merging Dave"

  # Merge in server
  # contains a CRL and new user certs
  MERGE_ID=server
  echo "$SCRIPTNAME: Merging in new user "
  merge_cmd server --source-dir ${P_R_SERVERDIR} -d ${PROFILE} -f ${R_PWFILE} -@ ${R_PWFILE}
  html_msg $? 0 "Merging server"

  # Merge in ext_client
  # contains a new certificate chain and additional trust flags
  MERGE_ID=ext_client
  echo "$SCRIPTNAME: Merging in new chain "
  merge_cmd ext_client --source-dir ${P_R_EXT_CLIENTDIR} -d ${PROFILE} -f ${R_PWFILE} -@ ${R_PWFILE}
  html_msg $? 0 "Merging ext_client"

  # Merge in SDR
  # contains a secret SDR key
  MERGE_ID=SDR
  echo "$SCRIPTNAME: Merging in SDR "
  merge_cmd sdr --source-dir ${P_R_SDR} -d ${PROFILE} -f ${R_PWFILE} -@ ${R_PWFILE}
  html_msg $? 0 "Merging SDR"

  # insert a listing of the database into the log for diagonic purposes
  ${BINDIR}/certutil -L -d ${PROFILE}
  ${BINDIR}/crlutil -L -d ${PROFILE}

  # Make sure we can decrypt with our original SDR key generated above
  echo "$SCRIPTNAME: Decrypt - With Original SDR Key"
  echo "sdrtest -d ${PROFILE} -i ${VALUE3} -t Test2 -f ${R_PWFILE}"
  ${PROFTOOL} ${BINDIR}/sdrtest -d ${PROFILE} -i ${VALUE3} -t Test2 -f ${R_PWFILE}
  html_msg $? 0 "Decrypt - Value 3"

  # Make sure we can decrypt with our the SDR key merged in from ../SDR
  echo "$SCRIPTNAME: Decrypt - With Merged SDR Key"
  echo "sdrtest -d ${PROFILE} -i ${VALUE1} -t Test1 -f ${R_PWFILE}"
  ${PROFTOOL} ${BINDIR}/sdrtest -d ${PROFILE} -i ${VALUE1} -t Test1 -f ${R_PWFILE}
  html_msg $? 0 "Decrypt - Value 1"

  # Make sure we can sign with merge certificate
  echo "$SCRIPTNAME: Signing with merged key  ------------------"
  echo "cmsutil -S -T -N Dave -H SHA1 -i alice.txt -d ${PROFILE} -p nss -o dave.dsig"
  ${PROFTOOL} ${BINDIR}/cmsutil -S -T -N Dave -H SHA1 -i alice.txt -d ${PROFILE} -p nss -o dave.dsig
  html_msg $? 0 "Create Detached Signature Dave" "."

  echo "cmsutil -D -i dave.dsig -c alice.txt -d ${PROFILE} "
  ${PROFTOOL} ${BINDIR}/cmsutil -D -i dave.dsig -c alice.txt -d ${PROFILE}
  html_msg $? 0 "Verifying Dave's Detached Signature"

  # Make sure that trust objects were properly merged
  echo "$SCRIPTNAME: verifying  merged cert  ------------------"
  echo "certutil -V -n ExtendedSSLUser -u C -d ${PROFILE}"
  ${PROFTOOL} ${BINDIR}/certutil -V -n ExtendedSSLUser -u C -d ${PROFILE}
  html_msg $? 0 "Verifying ExtendedSSL User Cert"

  # Make sure that the crl got properly copied in
  echo "$SCRIPTNAME: verifying  merged crl  ------------------"
  echo "crlutil -L -n TestCA -d ${PROFILE}"
  ${PROFTOOL} ${BINDIR}/crlutil -L -n TestCA -d ${PROFILE}
  html_msg $? 0 "Verifying TestCA CRL"

}
  
############################## smime_cleanup ###########################
# local shell function to finish this script (no exit since it might be
# sourced)
########################################################################
merge_cleanup()
{
  html "</TABLE><BR>"
  cd ${QADIR}
  . common/cleanup.sh
}

################## main #################################################

merge_init
merge_main
merge_cleanup

