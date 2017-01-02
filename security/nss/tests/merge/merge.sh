#! /bin/bash  
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

  # create a set of conflicting names.
  CONFLICT1DIR=conflict1
  CONFLICT2DIR=conflict2
  mkdir ${CONFLICT1DIR}
  mkdir ${CONFLICT2DIR}
  # in the upgrade mode (dbm->sql), make sure our test databases
  # are dbm databases.
  if [ "${TEST_MODE}" = "UPGRADE_DB" ]; then
	save=${NSS_DEFAULT_DB_TYPE}
	NSS_DEFAULT_DB_TYPE= ; export NSS_DEFAULT_DB_TYPE
  fi

  certutil -N -d ${CONFLICT1DIR} -f ${R_PWFILE}
  certutil -N -d ${CONFLICT2DIR} -f ${R_PWFILE}
  certutil -A -n Alice -t ,, -i ${R_CADIR}/TestUser41.cert -d ${CONFLICT1DIR}
  # modify CONFLICTDIR potentially corrupting the database
  certutil -A -n "Alice #1" -t C,, -i ${R_CADIR}/TestUser42.cert -d ${CONFLICT1DIR} -f ${R_PWFILE}
  certutil -M -n "Alice #1" -t ,, -d ${CONFLICT1DIR} -f ${R_PWFILE}
  certutil -A -n "Alice #99" -t ,, -i ${R_CADIR}/TestUser43.cert -d ${CONFLICT1DIR}
  certutil -A -n Alice -t ,, -i ${R_CADIR}/TestUser44.cert -d ${CONFLICT2DIR}
  certutil -A -n "Alice #1" -t ,, -i ${R_CADIR}/TestUser45.cert -d ${CONFLICT2DIR}
  certutil -A -n "Alice #99" -t ,, -i ${R_CADIR}/TestUser46.cert -d ${CONFLICT2DIR}
  if [ "${TEST_MODE}" = "UPGRADE_DB" ]; then
	NSS_DEFAULT_DB_TYPE=${save}; export NSS_DEFAULT_DB_TYPE
  fi

  #
  # allow all the tests to run in standalone mode.
  #  in standalone mode, TEST_MODE is not set.
  #  if NSS_DEFAULT_DB_TYPE is dbm, then test merge with dbm
  #  if NSS_DEFAULT_DB_TYPE is sql, then test merge with sql
  #  if NSS_DEFAULT_DB_TYPE is not set, then test database upgrade merge
  #   from dbm databases (created above) into a new sql db.
  if [ -z "${TEST_MODE}" ] && [ ${HAS_EXPLICIT_DB} -eq 0 ]; then
	echo "*** Using Standalone Upgrade DB mode"
	NSS_DEFAULT_DB_TYPE=sql; export NSS_DEFAULT_DB_TYPE
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

  # Merge conflicting nicknames in conflict1dir
  # contains several certificates with nicknames that conflict with the target
  # database
  MERGE_ID=conflict1
  echo "$SCRIPTNAME: Merging in conflicting nicknames 1"
  merge_cmd conflict1 --source-dir ${CONFLICT1DIR} -d ${PROFILE} -f ${R_PWFILE} -@ ${R_PWFILE}

  html_msg $? 0 "Merging conflicting nicknames 1"

  # Merge conflicting nicknames in conflict2dir
  # contains several certificates with nicknames that conflict with the target
  # database
  MERGE_ID=conflict2
  echo "$SCRIPTNAME: Merging in conflicting nicknames 1"
  merge_cmd conflict2 --source-dir ${CONFLICT2DIR} -d ${PROFILE} -f ${R_PWFILE} -@ ${R_PWFILE}
  html_msg $? 0 "Merging conflicting nicknames 2"

  # Make sure conflicted names were properly sorted out.
  echo "$SCRIPTNAME: Verify nicknames were deconflicted (Alice #4)"
  certutil -L -n "Alice #4" -d ${PROFILE}
  html_msg $? 0 "Verify nicknames were deconflicted (Alice #4)"

  # Make sure conflicted names were properly sorted out.
  echo "$SCRIPTNAME: Verify nicknames were deconflicted (Alice #100)"
  certutil -L -n "Alice #100" -d ${PROFILE}
  html_msg $? 0 "Verify nicknames were deconflicted (Alice #100)"

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
echo "TEST_MODE=${TEST_MODE}"
echo "NSS_DEFAULT_DB_TYPE=${NSS_DEFAULT_DB_TYPE}"
merge_cleanup


