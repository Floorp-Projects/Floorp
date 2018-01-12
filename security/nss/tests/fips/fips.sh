#! /bin/bash  
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
# mozilla/security/nss/tests/fips/fips.sh
#
# Script to test basic functionallity of NSS in FIPS-compliant mode
#
# needs to work on all Unix and Windows platforms
#
# tests implemented:
#
# special strings
# ---------------
#
########################################################################

############################## fips_init ##############################
# local shell function to initialize this script 
########################################################################
fips_init()
{
  SCRIPTNAME=fips.sh      # sourced - $0 would point to all.sh

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
  SCRIPTNAME=fips.sh
  html_head "FIPS 140 Compliance Tests"

  grep "SUCCESS: FIPS passed" $CERT_LOG_FILE >/dev/null || {
      Exit 15 "Fatal - FIPS of cert.sh needs to pass first"
  }

  COPYDIR=${FIPSDIR}/copydir

  R_FIPSDIR=../fips
  P_R_FIPSDIR=../fips
  R_COPYDIR=../fips/copydir

  if [ -n "${MULTIACCESS_DBM}" ]; then
     P_R_FIPSDIR="multiaccess:${D_FIPS}"
  fi

  mkdir -p ${FIPSDIR}
  mkdir -p ${COPYDIR}

  cd ${FIPSDIR}
}

############################## fips_140 ##############################
# local shell function to test basic functionality of NSS while in
# FIPS 140 compliant mode
########################################################################
fips_140()
{
  echo "$SCRIPTNAME: Verify this module is in FIPS mode  -----------------"
  echo "modutil -dbdir ${P_R_FIPSDIR} -list"
  ${BINDIR}/modutil -dbdir ${P_R_FIPSDIR} -list 2>&1
  ${BINDIR}/modutil -dbdir ${P_R_FIPSDIR} -chkfips true 2>&1
  html_msg $? 0 "Verify this module is in FIPS mode (modutil -chkfips true)" "."

  echo "$SCRIPTNAME: List the FIPS module certificates -----------------"
  echo "certutil -d ${P_R_FIPSDIR} -L"
  ${BINDIR}/certutil -d ${P_R_FIPSDIR} -L 2>&1
  html_msg $? 0 "List the FIPS module certificates (certutil -L)" "."

  echo "$SCRIPTNAME: List the FIPS module keys -------------------------"
  echo "certutil -d ${P_R_FIPSDIR} -K -f ${R_FIPSPWFILE}"
  ${BINDIR}/certutil -d ${P_R_FIPSDIR} -K -f ${R_FIPSPWFILE} 2>&1
  html_msg $? 0 "List the FIPS module keys (certutil -K)" "."

  echo "$SCRIPTNAME: Attempt to list FIPS module keys with incorrect password"
  echo "certutil -d ${P_R_FIPSDIR} -K -f ${FIPSBADPWFILE}"
  ${BINDIR}/certutil -d ${P_R_FIPSDIR} -K -f ${FIPSBADPWFILE} 2>&1
  RET=$?
  html_msg $RET 255 "Attempt to list FIPS module keys with incorrect password (certutil -K)" "."
  echo "certutil -K returned $RET"

  echo "$SCRIPTNAME: Validate the certificate --------------------------"
  echo "certutil -d ${P_R_FIPSDIR} -V -n ${FIPSCERTNICK} -u SR -e -f ${R_FIPSPWFILE}"
  ${BINDIR}/certutil -d ${P_R_FIPSDIR} -V -n ${FIPSCERTNICK} -u SR -e -f ${R_FIPSPWFILE}
  html_msg $? 0 "Validate the certificate (certutil -V -e)" "."

  echo "$SCRIPTNAME: Export the certificate and key as a PKCS#12 file --"
  echo "pk12util -d ${P_R_FIPSDIR} -o fips140.p12 -n ${FIPSCERTNICK} -w ${R_FIPSP12PWFILE} -k ${R_FIPSPWFILE}"
  ${BINDIR}/pk12util -d ${P_R_FIPSDIR} -o fips140.p12 -n ${FIPSCERTNICK} -w ${R_FIPSP12PWFILE} -k ${R_FIPSPWFILE} 2>&1
  html_msg $? 0 "Export the certificate and key as a PKCS#12 file (pk12util -o)" "."

  echo "$SCRIPTNAME: Export the certificate as a DER-encoded file ------"
  echo "certutil -d ${P_R_FIPSDIR} -L -n ${FIPSCERTNICK} -r -o fips140.crt"
  ${BINDIR}/certutil -d ${P_R_FIPSDIR} -L -n ${FIPSCERTNICK} -r -o fips140.crt 2>&1
  html_msg $? 0 "Export the certificate as a DER (certutil -L -r)" "."

  echo "$SCRIPTNAME: List the FIPS module certificates -----------------"
  echo "certutil -d ${P_R_FIPSDIR} -L"
  certs=`${BINDIR}/certutil -d ${P_R_FIPSDIR} -L 2>&1`
  ret=$?
  echo "${certs}" 
  if [ ${ret} -eq 0 ]; then
    echo "${certs}" | grep FIPS_PUB_140_Test_Certificate > /dev/null
    ret=$?
  fi
  html_msg $ret 0 "List the FIPS module certificates (certutil -L)" "."


  echo "$SCRIPTNAME: Delete the certificate and key from the FIPS module"
  echo "certutil -d ${P_R_FIPSDIR} -F -n ${FIPSCERTNICK} -f ${R_FIPSPWFILE}"
  ${BINDIR}/certutil -d ${P_R_FIPSDIR} -F -n ${FIPSCERTNICK} -f ${R_FIPSPWFILE} 2>&1
  html_msg $? 0 "Delete the certificate and key from the FIPS module (certutil -F)" "."

  echo "$SCRIPTNAME: List the FIPS module certificates -----------------"
  echo "certutil -d ${P_R_FIPSDIR} -L"
  certs=`${BINDIR}/certutil -d ${P_R_FIPSDIR} -L 2>&1`
  ret=$?
  echo "${certs}" 
  if [ ${ret} -eq 0 ]; then
    echo "${certs}" | grep FIPS_PUB_140_Test_Certificate > /dev/null
    if [ $? -eq 0 ]; then
      ret=255
    fi
  fi
  html_msg $ret 0 "List the FIPS module certificates (certutil -L)" "."

  echo "$SCRIPTNAME: List the FIPS module keys."
  echo "certutil -d ${P_R_FIPSDIR} -K -f ${R_FIPSPWFILE}"
  ${BINDIR}/certutil -d ${P_R_FIPSDIR} -K -f ${R_FIPSPWFILE} 2>&1
  # certutil -K now returns a failure if no keys are found. This verifies that
  # our delete succeded.
  html_msg $? 255 "List the FIPS module keys (certutil -K)" "."


  echo "$SCRIPTNAME: Import the certificate and key from the PKCS#12 file"
  echo "pk12util -d ${P_R_FIPSDIR} -i fips140.p12 -w ${R_FIPSP12PWFILE} -k ${R_FIPSPWFILE}"
  ${BINDIR}/pk12util -d ${P_R_FIPSDIR} -i fips140.p12 -w ${R_FIPSP12PWFILE} -k ${R_FIPSPWFILE} 2>&1
  html_msg $? 0 "Import the certificate and key from the PKCS#12 file (pk12util -i)" "."

  echo "$SCRIPTNAME: List the FIPS module certificates -----------------"
  echo "certutil -d ${P_R_FIPSDIR} -L"
  certs=`${BINDIR}/certutil -d ${P_R_FIPSDIR} -L 2>&1`
  ret=$?
  echo "${certs}" 
  if [ ${ret} -eq 0 ]; then
    echo "${certs}" | grep FIPS_PUB_140_Test_Certificate > /dev/null
    ret=$?
  fi
  html_msg $ret 0 "List the FIPS module certificates (certutil -L)" "."

  echo "$SCRIPTNAME: List the FIPS module keys --------------------------"
  echo "certutil -d ${P_R_FIPSDIR} -K -f ${R_FIPSPWFILE}"
  ${BINDIR}/certutil -d ${P_R_FIPSDIR} -K -f ${R_FIPSPWFILE} 2>&1
  html_msg $? 0 "List the FIPS module keys (certutil -K)" "."


  echo "$SCRIPTNAME: Delete the certificate from the FIPS module"
  echo "certutil -d ${P_R_FIPSDIR} -D -n ${FIPSCERTNICK}"
  ${BINDIR}/certutil -d ${P_R_FIPSDIR} -D -n ${FIPSCERTNICK} 2>&1
  html_msg $? 0 "Delete the certificate from the FIPS module (certutil -D)" "."

  echo "$SCRIPTNAME: List the FIPS module certificates -----------------"
  echo "certutil -d ${P_R_FIPSDIR} -L"
  certs=`${BINDIR}/certutil -d ${P_R_FIPSDIR} -L 2>&1`
  ret=$?
  echo "${certs}" 
  if [ ${ret} -eq 0 ]; then
    echo "${certs}" | grep FIPS_PUB_140_Test_Certificate > /dev/null
    if [ $? -eq 0 ]; then
      ret=255
    fi
  fi
  html_msg $ret 0 "List the FIPS module certificates (certutil -L)" "."


  echo "$SCRIPTNAME: Import the certificate and key from the PKCS#12 file"
  echo "pk12util -d ${P_R_FIPSDIR} -i fips140.p12 -w ${R_FIPSP12PWFILE} -k ${R_FIPSPWFILE}"
  ${BINDIR}/pk12util -d ${P_R_FIPSDIR} -i fips140.p12 -w ${R_FIPSP12PWFILE} -k ${R_FIPSPWFILE} 2>&1
  html_msg $? 0 "Import the certificate and key from the PKCS#12 file (pk12util -i)" "."

  echo "$SCRIPTNAME: List the FIPS module certificates -----------------"
  echo "certutil -d ${P_R_FIPSDIR} -L"
  certs=`${BINDIR}/certutil -d ${P_R_FIPSDIR} -L 2>&1`
  ret=$?
  echo "${certs}" 
  if [ ${ret} -eq 0 ]; then
    echo "${certs}" | grep FIPS_PUB_140_Test_Certificate > /dev/null
    ret=$?
  fi
  html_msg $ret 0 "List the FIPS module certificates (certutil -L)" "."

  echo "$SCRIPTNAME: List the FIPS module keys --------------------------"
  echo "certutil -d ${P_R_FIPSDIR} -K -f ${R_FIPSPWFILE}"
  ${BINDIR}/certutil -d ${P_R_FIPSDIR} -K -f ${R_FIPSPWFILE} 2>&1
  html_msg $? 0 "List the FIPS module keys (certutil -K)" "."


  echo "$SCRIPTNAME: Run PK11MODE in FIPSMODE  -----------------"
  echo "pk11mode -d ${P_R_FIPSDIR} -p fips- -f ${R_FIPSPWFILE}"
  ${BINDIR}/pk11mode -d ${P_R_FIPSDIR} -p fips- -f ${R_FIPSPWFILE}  2>&1
  html_msg $? 0 "Run PK11MODE in FIPS mode (pk11mode)" "."

  echo "$SCRIPTNAME: Run PK11MODE in Non FIPSMODE  -----------------"
  echo "pk11mode -d ${P_R_FIPSDIR} -p nonfips- -f ${R_FIPSPWFILE} -n"
  ${BINDIR}/pk11mode -d ${P_R_FIPSDIR} -p nonfips- -f ${R_FIPSPWFILE} -n 2>&1
  html_msg $? 0 "Run PK11MODE in Non FIPS mode (pk11mode -n)" "."

  LIBDIR="${DIST}/${OBJDIR}/lib"
  MANGLEDIR="${FIPSDIR}/mangle"
   
  # There are different versions of cp command on different systems, some of them 
  # copies only symlinks, others doesn't have option to disable links, so there
  # is needed to copy files one by one. 
  echo "mkdir ${MANGLEDIR}"
  mkdir ${MANGLEDIR}
  for lib in `ls ${LIBDIR}`; do
    echo "cp ${LIBDIR}/${lib} ${MANGLEDIR}"
    cp ${LIBDIR}/${lib} ${MANGLEDIR}
  done
    
  echo "$SCRIPTNAME: Detect mangled softoken--------------------------"
  SOFTOKEN=${MANGLEDIR}/${DLL_PREFIX}softokn3.${DLL_SUFFIX}

  echo "mangling ${SOFTOKEN}"
  echo "mangle -i ${SOFTOKEN} -o -8 -b 5"
  # If nss was built without softoken use the system installed one.
  # It's location must be specified by the package maintainer.
  if [ ! -e  ${MANGLEDIR}/${DLL_PREFIX}softokn3.${DLL_SUFFIX} ]; then
    echo "cp ${SOFTOKEN_LIB_DIR}/${DLL_PREFIX}softokn3.${DLL_SUFFIX} ${MANGLEDIR}"
    cp ${SOFTOKEN_LIB_DIR}/${DLL_PREFIX}softokn3.${DLL_SUFFIX} ${MANGLEDIR}
  fi
  ${BINDIR}/mangle -i ${SOFTOKEN} -o -8 -b 5 2>&1
  if [ $? -eq 0 ]; then
    if [ "${OS_ARCH}" = "WINNT" ]; then
      DBTEST=`which dbtest`
	  if [ "${OS_ARCH}" = "WINNT" -a "$OS_NAME" = "CYGWIN_NT" ]; then
		DBTEST=`cygpath -m ${DBTEST}`
		MANGLEDIR=`cygpath -u ${MANGLEDIR}`
	  fi
      echo "PATH=${MANGLEDIR} ${DBTEST} -r -d ${P_R_FIPSDIR}"
      PATH="${MANGLEDIR}" ${DBTEST} -r -d ${P_R_FIPSDIR} > ${TMP}/dbtestoutput.txt 2>&1
      RESULT=$?
    elif [ "${OS_ARCH}" = "HP-UX" ]; then
      echo "SHLIB_PATH=${MANGLEDIR} dbtest -r -d ${P_R_FIPSDIR}"
      LD_LIBRARY_PATH="" SHLIB_PATH="${MANGLEDIR}" ${BINDIR}/dbtest -r -d ${P_R_FIPSDIR} > ${TMP}/dbtestoutput.txt 2>&1
      RESULT=$?
    elif [ "${OS_ARCH}" = "AIX" ]; then
      echo "LIBPATH=${MANGLEDIR} dbtest -r -d ${P_R_FIPSDIR}"
      LIBPATH="${MANGLEDIR}" ${BINDIR}/dbtest -r -d ${P_R_FIPSDIR} > ${TMP}/dbtestoutput.txt 2>&1
      RESULT=$?
    elif [ "${OS_ARCH}" = "Darwin" ]; then
      echo "DYLD_LIBRARY_PATH=${MANGLEDIR} dbtest -r -d ${P_R_FIPSDIR}"
      DYLD_LIBRARY_PATH="${MANGLEDIR}" ${BINDIR}/dbtest -r -d ${P_R_FIPSDIR} > ${TMP}/dbtestoutput.txt 2>&1
      RESULT=$?
    else
      echo "LD_LIBRARY_PATH=${MANGLEDIR} dbtest -r -d ${P_R_FIPSDIR}"
      LD_LIBRARY_PATH="${MANGLEDIR}" ${BINDIR}/dbtest -r -d ${P_R_FIPSDIR} > ${TMP}/dbtestoutput.txt 2>&1
      RESULT=$?
    fi  

    html_msg ${RESULT} 46 "Init NSS with a corrupted library (dbtest -r)" "."
  else
    html_failed "Mangle ${DLL_PREFIX}softokn3.${DLL_SUFFIX}"
  fi
}

############################## fips_cleanup ############################
# local shell function to finish this script (no exit since it might be 
# sourced)
########################################################################
fips_cleanup()
{
  html "</TABLE><BR>"
  cd ${QADIR}
  . common/cleanup.sh
}

################## main #################################################

fips_init
fips_140
fips_cleanup
echo "fips.sh done"
