#! /bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
#
# mozilla/security/nss/tests/ssl/ssl.sh
#
# Script to test NSS SSL
#
# needs to work on all Unix and Windows platforms
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
########################################################################

############################## ssl_init ################################
# local shell function to initialize this script
########################################################################
ssl_init()
{
  SCRIPTNAME=ssl.sh      # sourced - $0 would point to all.sh

  if [ -z "${CLEANUP}" ] ; then     # if nobody else is responsible for
      CLEANUP="${SCRIPTNAME}"       # cleaning this script will do it
  fi

  if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ]; then
      cd ../common
      . ./init.sh
  fi
  if [ -z "${IOPR_SSL_SOURCED}" ]; then
      . ../iopr/ssl_iopr.sh
  fi
  if [ ! -r $CERT_LOG_FILE ]; then  # we need certificates here
      cd ../cert
      . ./cert.sh
  fi
  SCRIPTNAME=ssl.sh
  echo "$SCRIPTNAME: SSL tests ==============================="

  grep "SUCCESS: SSL passed" $CERT_LOG_FILE >/dev/null || {
      html_head "SSL Test failure"
      Exit 8 "Fatal - cert.sh needs to pass first"
  }

  if [ -z "$NSS_TEST_DISABLE_CRL" ] ; then
      grep "SUCCESS: SSL CRL prep passed" $CERT_LOG_FILE >/dev/null || {
          html_head "SSL Test failure"
          Exit 8 "Fatal - SSL of cert.sh needs to pass first"
      }
  fi

  PORT=${PORT-8443}
  # Avoid port conflicts when multiple tests are running on the same machine.
  if [ -n "$NSS_TASKCLUSTER_MAC" ]; then
    cwd=$(cd $(dirname $0); pwd -P)
    padd=$(echo $cwd | cut -d "/" -f4 | sed 's/[^0-9]//g')
    PORT=$(($PORT + $padd))
  fi
  NSS_SSL_TESTS=${NSS_SSL_TESTS:-normal_normal}
  nss_ssl_run="stapling signed_cert_timestamps cov auth stress dtls"
  NSS_SSL_RUN=${NSS_SSL_RUN:-$nss_ssl_run}
  
  # Test case files
  SSLCOV=${QADIR}/ssl/sslcov.txt
  SSLAUTH=${QADIR}/ssl/sslauth.txt
  SSLSTRESS=${QADIR}/ssl/sslstress.txt
  SSLPOLICY=${QADIR}/ssl/sslpolicy.txt
  REQUEST_FILE=${QADIR}/ssl/sslreq.dat

  #temparary files
  SERVEROUTFILE=${TMP}/tests_server.$$
  SERVERPID=${TMP}/tests_pid.$$

  R_SERVERPID=../tests_pid.$$

  TEMPFILES="$TMPFILES ${SERVEROUTFILE}  ${SERVERPID}"

  fileout=0 #FIXME, looks like all.sh tried to turn this on but actually didn't
  #fileout=1
  #verbose="-v" #FIXME - see where this is usefull

  USER_NICKNAME=TestUser
  NORM_EXT=""

  EC_SUITES=":C001:C002:C003:C004:C005:C006:C007:C008:C009:C00A:C00B:C00C:C00D"
  EC_SUITES="${EC_SUITES}:C00E:C00F:C010:C011:C012:C013:C014:C023:C024:C027"
  EC_SUITES="${EC_SUITES}:C028:C02B:C02C:C02F:C030:CCA8:CCA9:CCAA"

  NON_EC_SUITES=":0016:0032:0033:0038:0039:003B:003C:003D:0040:0041:0067:006A:006B"
  NON_EC_SUITES="${NON_EC_SUITES}:0084:009C:009D:009E:009F:00A2:00A3:CCAAcdeinvyz"

  # List of cipher suites to test, including ECC cipher suites.
  CIPHER_SUITES="-c ${EC_SUITES}${NON_EC_SUITES}"

  if [ "${OS_ARCH}" != "WINNT" ]; then
      ulimit -n 1000 # make sure we have enough file descriptors
  fi

  cd ${CLIENTDIR}
}

########################### is_selfserv_alive ##########################
# local shell function to exit with a fatal error if selfserver is not
# running
########################################################################
is_selfserv_alive()
{
  if [ ! -f "${SERVERPID}" ]; then
      echo "$SCRIPTNAME: Error - selfserv PID file ${SERVERPID} doesn't exist"
      sleep 5
      if [ ! -f "${SERVERPID}" ]; then
          Exit 9 "Fatal - selfserv pid file ${SERVERPID} does not exist"
      fi
  fi

  if [ "${OS_ARCH}" = "WINNT" ] && \
     [ "$OS_NAME" = "CYGWIN_NT" -o "$OS_NAME" = "MINGW32_NT" ]; then
      PID=${SHELL_SERVERPID}
  else
      PID=`cat ${SERVERPID}`
  fi

  echo "kill -0 ${PID} >/dev/null 2>/dev/null"
  kill -0 ${PID} >/dev/null 2>/dev/null || Exit 10 "Fatal - selfserv process not detectable"

  echo "selfserv with PID ${PID} found at `date`"
}

########################### wait_for_selfserv ##########################
# local shell function to wait until selfserver is running and initialized
########################################################################
wait_for_selfserv()
{
  #verbose="-v"
  echo "trying to connect to selfserv at `date`"
  echo "tstclnt -4 -p ${PORT} -h ${HOSTADDR} ${CLIENT_OPTIONS} -q \\"
  echo "        -d ${P_R_CLIENTDIR} $verbose < ${REQUEST_FILE}"
  ${BINDIR}/tstclnt -4 -p ${PORT} -h ${HOSTADDR} ${CLIENT_OPTIONS} -q \
          -d ${P_R_CLIENTDIR} $verbose < ${REQUEST_FILE}
  if [ $? -ne 0 ]; then
      sleep 5
      echo "retrying to connect to selfserv at `date`"
      echo "tstclnt -p ${PORT} -h ${HOSTADDR} ${CLIENT_OPTIONS} -q \\"
      echo "        -d ${P_R_CLIENTDIR} $verbose < ${REQUEST_FILE}"
      ${BINDIR}/tstclnt -4 -p ${PORT} -h ${HOSTADDR} ${CLIENT_OPTIONS} -q \
              -d ${P_R_CLIENTDIR} $verbose < ${REQUEST_FILE}
      if [ $? -ne 0 ]; then
          html_failed "Waiting for Server"
      fi
  fi
  is_selfserv_alive
}

########################### kill_selfserv ##############################
# local shell function to kill the selfserver after the tests are done
########################################################################
kill_selfserv()
{
  if [ "${OS_ARCH}" = "WINNT" ] && \
     [ "$OS_NAME" = "CYGWIN_NT" -o "$OS_NAME" = "MINGW32_NT" ]; then
      PID=${SHELL_SERVERPID}
  else
      PID=`cat ${SERVERPID}`
  fi

  echo "trying to kill selfserv with PID ${PID} at `date`"

  if [ "${OS_ARCH}" = "WINNT" -o "${OS_ARCH}" = "WIN95" -o "${OS_ARCH}" = "OS2" ]; then
      echo "${KILL} ${PID}"
      ${KILL} ${PID}
  else
      echo "${KILL} -USR1 ${PID}"
      ${KILL} -USR1 ${PID}
  fi
  wait ${PID}
  if [ ${fileout} -eq 1 ]; then
      cat ${SERVEROUTFILE}
  fi

  # On Linux selfserv needs up to 30 seconds to fully die and free
  # the port.  Wait until the port is free. (Bug 129701)
  if [ "${OS_ARCH}" = "Linux" ]; then
      echo "selfserv -b -p ${PORT} 2>/dev/null;"
      until ${BINDIR}/selfserv -b -p ${PORT} 2>/dev/null; do
          echo "RETRY: selfserv -b -p ${PORT} 2>/dev/null;"
          sleep 1
      done
  fi

  echo "selfserv with PID ${PID} killed at `date`"

  rm ${SERVERPID}
  html_detect_core "kill_selfserv core detection step"
}

########################### start_selfserv #############################
# local shell function to start the selfserver with the parameters required
# for this test and log information (parameters, start time)
# also: wait until the server is up and running
########################################################################
start_selfserv()
{
  if [ -n "$testname" ] ; then
      echo "$SCRIPTNAME: $testname ----"
  fi
  sparam=`echo $sparam | sed -e 's;_; ;g'`
  if [ -z "$NO_ECC_CERTS" -o "$NO_ECC_CERTS" != "1"  ] ; then
      ECC_OPTIONS="-e ${HOSTADDR}-ecmixed -e ${HOSTADDR}-ec"
  else
      ECC_OPTIONS=""
  fi
  echo "selfserv starting at `date`"
  echo "selfserv -D -p ${PORT} -d ${P_R_SERVERDIR} -n ${HOSTADDR} ${SERVER_OPTIONS} \\"
  echo "         ${ECC_OPTIONS} -S ${HOSTADDR}-dsa -w nss ${sparam} -i ${R_SERVERPID}\\"
  echo "         -V ssl3:tls1.2 $verbose -H 1 &"
  if [ ${fileout} -eq 1 ]; then
      ${PROFTOOL} ${BINDIR}/selfserv -D -p ${PORT} -d ${P_R_SERVERDIR} -n ${HOSTADDR} ${SERVER_OPTIONS} \
               ${ECC_OPTIONS} -S ${HOSTADDR}-dsa -w nss ${sparam} -i ${R_SERVERPID} -V ssl3:tls1.2 $verbose -H 1 \
               > ${SERVEROUTFILE} 2>&1 &
      RET=$?
  else
      ${PROFTOOL} ${BINDIR}/selfserv -D -p ${PORT} -d ${P_R_SERVERDIR} -n ${HOSTADDR} ${SERVER_OPTIONS} \
               ${ECC_OPTIONS} -S ${HOSTADDR}-dsa -w nss ${sparam} -i ${R_SERVERPID} -V ssl3:tls1.2 $verbose -H 1 &
      RET=$?
  fi

  # The PID $! returned by the MKS or Cygwin shell is not the PID of
  # the real background process, but rather the PID of a helper
  # process (sh.exe).  MKS's kill command has a bug: invoking kill
  # on the helper process does not terminate the real background
  # process.  Our workaround has been to have selfserv save its PID
  # in the ${SERVERPID} file and "kill" that PID instead.  But this
  # doesn't work under Cygwin; its kill command doesn't recognize
  # the PID of the real background process, but it does work on the
  # PID of the helper process.  So we save the value of $! in the
  # SHELL_SERVERPID variable, and use it instead of the ${SERVERPID}
  # file under Cygwin.  (In fact, this should work in any shell
  # other than the MKS shell.)
  SHELL_SERVERPID=$!
  wait_for_selfserv

  if [ "${OS_ARCH}" = "WINNT" ] && \
     [ "$OS_NAME" = "CYGWIN_NT" -o "$OS_NAME" = "MINGW32_NT" ]; then
      PID=${SHELL_SERVERPID}
  else
      PID=`cat ${SERVERPID}`
  fi

  echo "selfserv with PID ${PID} started at `date`"
}

ignore_blank_lines()
{
  LC_ALL=C grep -v '^[[:space:]]*\(#\|$\)' "$1"
}

############################## ssl_cov #################################
# local shell function to perform SSL Cipher Coverage tests
########################################################################
ssl_cov()
{
  #verbose="-v"
  html_head "SSL Cipher Coverage $NORM_EXT - server $SERVER_MODE/client $CLIENT_MODE"

  testname=""
  sparam="$CIPHER_SUITES"

  start_selfserv # Launch the server

  VMIN="ssl3"
  VMAX="tls1.1"

  ignore_blank_lines ${SSLCOV} | \
  while read ectype testmax param testname
  do
      echo "${testname}" | grep "EXPORT" > /dev/null
      EXP=$?

      if [ "$ectype" = "ECC" ] ; then
          echo "$SCRIPTNAME: skipping  $testname (ECC only)"
      else
          echo "$SCRIPTNAME: running $testname ----------------------------"
          VMAX="ssl3"
          if [ "$testmax" = "TLS10" ]; then
              VMAX="tls1.0"
          fi
          if [ "$testmax" = "TLS11" ]; then
              VMAX="tls1.1"
          fi
          if [ "$testmax" = "TLS12" ]; then
              VMAX="tls1.2"
          fi

          echo "tstclnt -4 -p ${PORT} -h ${HOSTADDR} -c ${param} -V ${VMIN}:${VMAX} ${CLIENT_OPTIONS} \\"
          echo "        -f -d ${P_R_CLIENTDIR} $verbose -w nss < ${REQUEST_FILE}"

          rm ${TMP}/$HOST.tmp.$$ 2>/dev/null
          ${PROFTOOL} ${BINDIR}/tstclnt -4 -p ${PORT} -h ${HOSTADDR} -c ${param} -V ${VMIN}:${VMAX} ${CLIENT_OPTIONS} -f \
                  -d ${P_R_CLIENTDIR} $verbose -w nss < ${REQUEST_FILE} \
                  >${TMP}/$HOST.tmp.$$  2>&1
          ret=$?
          cat ${TMP}/$HOST.tmp.$$
          rm ${TMP}/$HOST.tmp.$$ 2>/dev/null
          html_msg $ret 0 "${testname}" \
                   "produced a returncode of $ret, expected is 0"
      fi
  done

  kill_selfserv
  html "</TABLE><BR>"
}

############################## ssl_auth ################################
# local shell function to perform SSL  Client Authentication tests
########################################################################
ssl_auth()
{
  #verbose="-v"
  html_head "SSL Client Authentication $NORM_EXT - server $SERVER_MODE/client $CLIENT_MODE"

  ignore_blank_lines ${SSLAUTH} | \
  while read ectype value sparam cparam testname
  do
      echo "${testname}" | grep "don't require client auth" > /dev/null
      CAUTH=$?

      if [ "${CLIENT_MODE}" = "fips" -a "${CAUTH}" -eq 0 ] ; then
          echo "$SCRIPTNAME: skipping  $testname (non-FIPS only)"
      elif [ "$ectype" = "SNI" -a "$NORM_EXT" = "Extended Test" ] ; then
          echo "$SCRIPTNAME: skipping  $testname for $NORM_EXT"
      elif [ "$ectype" = "ECC" ] ; then
          echo "$SCRIPTNAME: skipping  $testname (ECC only)"
      else
          cparam=`echo $cparam | sed -e 's;_; ;g' -e "s/TestUser/$USER_NICKNAME/g" `
          if [ "$ectype" = "SNI" ]; then
              cparam=`echo $cparam | sed -e "s/Host/$HOST/g" -e "s/Dom/$DOMSUF/g" `
              sparam=`echo $sparam | sed -e "s/Host/$HOST/g" -e "s/Dom/$DOMSUF/g" `
          fi
          start_selfserv

          echo "tstclnt -4 -p ${PORT} -h ${HOSTADDR} -f -d ${P_R_CLIENTDIR} $verbose ${CLIENT_OPTIONS} \\"
          echo "        ${cparam}  < ${REQUEST_FILE}"
          rm ${TMP}/$HOST.tmp.$$ 2>/dev/null
          ${PROFTOOL} ${BINDIR}/tstclnt -4 -p ${PORT} -h ${HOSTADDR} -f ${cparam} $verbose ${CLIENT_OPTIONS} \
                  -d ${P_R_CLIENTDIR} < ${REQUEST_FILE} \
                  >${TMP}/$HOST.tmp.$$  2>&1
          ret=$?
          cat ${TMP}/$HOST.tmp.$$
          rm ${TMP}/$HOST.tmp.$$ 2>/dev/null

          #workaround for bug #402058
          [ $ret -ne 0 ] && ret=1
          [ $value -ne 0 ] && value=1

          html_msg $ret $value "${testname}" \
                   "produced a returncode of $ret, expected is $value"
          kill_selfserv
      fi
  done

  html "</TABLE><BR>"
}

ssl_stapling_sub()
{
    #verbose="-v"
    testname=$1
    SO=$2
    value=$3

    if [ "$NORM_EXT" = "Extended Test" ] ; then
	# these tests use the ext_client directory for tstclnt,
	# which doesn't contain the required "TestCA" for server cert
	# verification, I don't know if it would be OK to add it...
	echo "$SCRIPTNAME: skipping  $testname for $NORM_EXT"
	return 0
    fi
    if [ "$SERVER_MODE" = "fips" -o "$CLIENT_MODE" = "fips" ] ; then
          echo "$SCRIPTNAME: skipping  $testname (non-FIPS only)"
	return 0
    fi

    SAVE_SERVER_OPTIONS=${SERVER_OPTIONS}
    SERVER_OPTIONS="${SERVER_OPTIONS} ${SO}"

    SAVE_P_R_SERVERDIR=${P_R_SERVERDIR}
    P_R_SERVERDIR=${P_R_SERVERDIR}/../stapling/

    echo "${testname}"

    start_selfserv

    echo "tstclnt -4 -p ${PORT} -h ${HOSTADDR} -f -d ${P_R_CLIENTDIR} $verbose ${CLIENT_OPTIONS} \\"
    echo "        -c v -T -O -F -M 1 -V ssl3:tls1.2 < ${REQUEST_FILE}"
    rm ${TMP}/$HOST.tmp.$$ 2>/dev/null
    ${PROFTOOL} ${BINDIR}/tstclnt -4 -p ${PORT} -h ${HOSTADDR} -f ${CLIENT_OPTIONS} \
	    -d ${P_R_CLIENTDIR} $verbose -c v -T -O -F -M 1 -V ssl3:tls1.2 < ${REQUEST_FILE} \
	    >${TMP}/$HOST.tmp.$$  2>&1
    ret=$?
    cat ${TMP}/$HOST.tmp.$$
    rm ${TMP}/$HOST.tmp.$$ 2>/dev/null

    # hopefully no workaround for bug #402058 needed here?
    # (see commands in ssl_auth

    html_msg $ret $value "${testname}" \
	    "produced a returncode of $ret, expected is $value"
    kill_selfserv

    SERVER_OPTIONS=${SAVE_SERVER_OPTIONS}
    P_R_SERVERDIR=${SAVE_P_R_SERVERDIR}
}

ssl_stapling_stress()
{
    testname="Stress OCSP stapling, server uses random status"
    SO="-A TestCA -T random"
    value=0

    if [ "$NORM_EXT" = "Extended Test" ] ; then
	# these tests use the ext_client directory for tstclnt,
	# which doesn't contain the required "TestCA" for server cert
	# verification, I don't know if it would be OK to add it...
	echo "$SCRIPTNAME: skipping  $testname for $NORM_EXT"
	return 0
    fi
    if [ "$SERVER_MODE" = "fips" -o "$CLIENT_MODE" = "fips" ] ; then
          echo "$SCRIPTNAME: skipping  $testname (non-FIPS only)"
	return 0
    fi

    SAVE_SERVER_OPTIONS=${SERVER_OPTIONS}
    SERVER_OPTIONS="${SERVER_OPTIONS} ${SO}"

    SAVE_P_R_SERVERDIR=${P_R_SERVERDIR}
    P_R_SERVERDIR=${P_R_SERVERDIR}/../stapling/

    echo "${testname}"
    start_selfserv

    echo "strsclnt -q -p ${PORT} -d ${P_R_CLIENTDIR} ${CLIENT_OPTIONS} -w nss \\"
    echo "         -c 1000 -V ssl3:tls1.2 -N -T $verbose ${HOSTADDR}"
    echo "strsclnt started at `date`"
    ${PROFTOOL} ${BINDIR}/strsclnt -q -p ${PORT} -d ${P_R_CLIENTDIR} ${CLIENT_OPTIONS} -w nss \
	    -c 1000 -V ssl3:tls1.2 -N -T $verbose ${HOSTADDR}
    ret=$?

    echo "strsclnt completed at `date`"
    html_msg $ret $value \
	    "${testname}" \
	    "produced a returncode of $ret, expected is $value."
    kill_selfserv

    SERVER_OPTIONS=${SAVE_SERVER_OPTIONS}
    P_R_SERVERDIR=${SAVE_P_R_SERVERDIR}
}

############################ ssl_stapling ##############################
# local shell function to perform SSL Cert Status (OCSP Stapling) tests
########################################################################
ssl_stapling()
{
  html_head "SSL Cert Status (OCSP Stapling) $NORM_EXT - server $SERVER_MODE/client $CLIENT_MODE"

  # tstclnt Exit code:
  # 0: have fresh and valid revocation data, status good
  # 1: cert failed to verify, prior to revocation checking
  # 2: missing, old or invalid revocation data
  # 3: have fresh and valid revocation data, status revoked

  # selfserv modes
  # good, revoked, unkown: Include locally signed response. Requires: -A
  # failure: Include OCSP failure status, such as "try later" (unsigned)
  # badsig: use a good status but with an invalid signature
  # corrupted: stapled cert status is an invalid block of data

  ssl_stapling_sub "OCSP stapling, signed response, good status"     "-A TestCA -T good"      0
  ssl_stapling_sub "OCSP stapling, signed response, revoked status"  "-A TestCA -T revoked"   3
  ssl_stapling_sub "OCSP stapling, signed response, unknown status"  "-A TestCA -T unknown"   2
  ssl_stapling_sub "OCSP stapling, unsigned failure response"        "-A TestCA -T failure"   2
  ssl_stapling_sub "OCSP stapling, good status, bad signature"       "-A TestCA -T badsig"    2
  ssl_stapling_sub "OCSP stapling, invalid cert status data"         "-A TestCA -T corrupted" 2
  ssl_stapling_sub "Valid cert, Server doesn't staple"               ""                       2

  ssl_stapling_stress

  html "</TABLE><BR>"
}

############################ ssl_signed_cert_timestamps #################
# local shell function to perform SSL Signed Certificate Timestamp tests
#########################################################################
ssl_signed_cert_timestamps()
{
  #verbose="-v"
  html_head "SSL Signed Certificate Timestamps $NORM_EXT - server $SERVER_MODE/client $CLIENT_MODE"

    testname="ssl_signed_cert_timestamps"
    value=0

    if [ "$SERVER_MODE" = "fips" -o "$CLIENT_MODE" = "fips" ] ; then
          echo "$SCRIPTNAME: skipping  $testname (non-FIPS only)"
        return 0
    fi

    echo "${testname}"

    start_selfserv

    # Since we don't have server-side support, this test only covers advertising the
    # extension in the client hello.
    echo "tstclnt -4 -p ${PORT} -h ${HOSTADDR} -f -d ${P_R_CLIENTDIR} $verbose ${CLIENT_OPTIONS} \\"
    echo "        -U -V tls1.0:tls1.2 < ${REQUEST_FILE}"
    rm ${TMP}/$HOST.tmp.$$ 2>/dev/null
    ${PROFTOOL} ${BINDIR}/tstclnt -4 -p ${PORT} -h ${HOSTADDR} -f ${CLIENT_OPTIONS} \
            -d ${P_R_CLIENTDIR} $verbose -U -V tls1.0:tls1.2 < ${REQUEST_FILE} \
            >${TMP}/$HOST.tmp.$$  2>&1
    ret=$?
    cat ${TMP}/$HOST.tmp.$$
    rm ${TMP}/$HOST.tmp.$$ 2>/dev/null

    html_msg $ret $value "${testname}" \
            "produced a returncode of $ret, expected is $value"
    kill_selfserv
  html "</TABLE><BR>"
}


############################## ssl_stress ##############################
# local shell function to perform SSL stress test
########################################################################
ssl_stress()
{
  html_head "SSL Stress Test $NORM_EXT - server $SERVER_MODE/client $CLIENT_MODE"

  ignore_blank_lines ${SSLSTRESS} | \
  while read ectype value sparam cparam testname
  do
      echo "${testname}" | grep "client auth" > /dev/null
      CAUTH=$?
      echo "${testname}" | grep "no login" > /dev/null
      NOLOGIN=$?

      if [ "$ectype" = "SNI" -a "$NORM_EXT" = "Extended Test" ] ; then
          echo "$SCRIPTNAME: skipping  $testname for $NORM_EXT"
      elif [ "$ectype" = "ECC" ] ; then
          echo "$SCRIPTNAME: skipping  $testname (ECC only)"
      elif [ "${CLIENT_MODE}" = "fips" -a "${CAUTH}" -ne 0 ] ; then
          echo "$SCRIPTNAME: skipping  $testname (non-FIPS only)"
      elif [ "${NOLOGIN}" -eq 0 ] && \
           [ "${CLIENT_MODE}" = "fips" -o "$NORM_EXT" = "Extended Test" ] ; then
          echo "$SCRIPTNAME: skipping  $testname for $NORM_EXT"
      else
          cparam=`echo $cparam | sed -e 's;_; ;g' -e "s/TestUser/$USER_NICKNAME/g" `
          if [ "$ectype" = "SNI" ]; then
              cparam=`echo $cparam | sed -e "s/Host/$HOST/g" -e "s/Dom/$DOMSUF/g" `
              sparam=`echo $sparam | sed -e "s/Host/$HOST/g" -e "s/Dom/$DOMSUF/g" `
          fi

          start_selfserv

          if [ "`uname -n`" = "sjsu" ] ; then
              echo "debugging disapering selfserv... ps -ef | grep selfserv"
              ps -ef | grep selfserv
          fi

          if [ "${NOLOGIN}" -eq 0 ] ; then
              dbdir=${P_R_NOLOGINDIR}
          else
              dbdir=${P_R_CLIENTDIR}
          fi

          echo "strsclnt -q -p ${PORT} -d ${dbdir} ${CLIENT_OPTIONS} -w nss $cparam \\"
          echo "         -V ssl3:tls1.2 $verbose ${HOSTADDR}"
          echo "strsclnt started at `date`"
          ${PROFTOOL} ${BINDIR}/strsclnt -q -p ${PORT} -d ${dbdir} ${CLIENT_OPTIONS} -w nss $cparam \
                   -V ssl3:tls1.2 $verbose ${HOSTADDR}
          ret=$?
          echo "strsclnt completed at `date`"
          html_msg $ret $value \
                   "${testname}" \
                   "produced a returncode of $ret, expected is $value. "
          if [ "`uname -n`" = "sjsu" ] ; then
              echo "debugging disapering selfserv... ps -ef | grep selfserv"
              ps -ef | grep selfserv
          fi
          kill_selfserv
      fi
  done

  html "</TABLE><BR>"
}

############################ ssl_crl_ssl ###############################
# local shell function to perform SSL test with/out revoked certs tests
########################################################################
ssl_crl_ssl()
{
  #verbose="-v"
  html_head "CRL SSL Client Tests $NORM_EXT"

  # Using First CRL Group for this test. There are $CRL_GRP_1_RANGE certs in it.
  # Cert number $UNREVOKED_CERT_GRP_1 was not revoked
  CRL_GROUP_BEGIN=$CRL_GRP_1_BEGIN
  CRL_GROUP_RANGE=$CRL_GRP_1_RANGE
  UNREVOKED_CERT=$UNREVOKED_CERT_GRP_1

  ignore_blank_lines ${SSLAUTH} | \
  while read ectype value sparam cparam testname
  do
    if [ "$ectype" = "ECC" ] ; then
        echo "$SCRIPTNAME: skipping $testname (ECC only)"
    elif [ "$ectype" = "SNI" ]; then
        continue
    else
	servarg=`echo $sparam | awk '{r=split($0,a,"-r") - 1;print r;}'`
	pwd=`echo $cparam | grep nss`
	user=`echo $cparam | grep TestUser`
	_cparam=$cparam
	case $servarg in
	    1) if [ -z "$pwd" -o -z "$user" ]; then
                 rev_modvalue=0
               else
	         rev_modvalue=254
               fi
               ;;
	    2) rev_modvalue=254 ;;
	    3) if [ -z "$pwd" -o -z "$user" ]; then
		rev_modvalue=0
		else
		rev_modvalue=1
		fi
		;;
	    4) rev_modvalue=1 ;;
	esac
	TEMP_NUM=0
	while [ $TEMP_NUM -lt $CRL_GROUP_RANGE ]
	  do
	  CURR_SER_NUM=`expr ${CRL_GROUP_BEGIN} + ${TEMP_NUM}`
	  TEMP_NUM=`expr $TEMP_NUM + 1`
	  USER_NICKNAME="TestUser${CURR_SER_NUM}"
	  cparam=`echo $_cparam | sed -e 's;_; ;g' -e "s/TestUser/$USER_NICKNAME/g" `
	  start_selfserv

	  echo "tstclnt -4 -p ${PORT} -h ${HOSTADDR} -f -d ${R_CLIENTDIR} $verbose \\"
	  echo "        ${cparam}  < ${REQUEST_FILE}"
	  rm ${TMP}/$HOST.tmp.$$ 2>/dev/null
	  ${PROFTOOL} ${BINDIR}/tstclnt -4 -p ${PORT} -h ${HOSTADDR} -f ${cparam} \
	      -d ${R_CLIENTDIR} $verbose < ${REQUEST_FILE} \
	      >${TMP}/$HOST.tmp.$$  2>&1
	  ret=$?
	  cat ${TMP}/$HOST.tmp.$$
	  rm ${TMP}/$HOST.tmp.$$ 2>/dev/null
	  if [ $CURR_SER_NUM -ne $UNREVOKED_CERT ]; then
	      modvalue=$rev_modvalue
              testAddMsg="revoked"
	  else
              testAddMsg="not revoked"
	      modvalue=$value
	  fi

	  html_msg $ret $modvalue "${testname} (cert ${USER_NICKNAME} - $testAddMsg)" \
		"produced a returncode of $ret, expected is $modvalue"
	  kill_selfserv
	done
    fi
  done

  html "</TABLE><BR>"
}

############################# setup_policy #############################
# local shell function to create policy configuration
########################################################################
setup_policy()
{
  policy="$1"
  outdir="$2"
  OUTFILE="${outdir}/pkcs11.txt"
  cat > "$OUTFILE" << ++EOF++
library=
name=NSS Internal PKCS #11 Module
parameters=configdir='./client' certPrefix='' keyPrefix='' secmod='secmod.db' flags= updatedir='' updateCertPrefix='' updateKeyPrefix='' updateid='' updateTokenDescription=''
NSS=Flags=internal,critical trustOrder=75 cipherOrder=100 slotParams=(1={slotFlags=[RSA,DSA,DH,RC2,RC4,DES,RANDOM,SHA1,MD5,MD2,SSL,TLS,AES,Camellia,SEED,SHA256,SHA512] askpw=any timeout=30})
++EOF++
  echo "config=${policy}" >> "$OUTFILE"
  echo "" >> "$OUTFILE"
  echo "library=${DIST}/${OBJDIR}/lib/libnssckbi.so" >> "$OUTFILE"
  cat >> "$OUTFILE" << ++EOF++
name=RootCerts
NSS=trustOrder=100
++EOF++

  echo "******************************Testing with: "
  cat "$OUTFILE"
  echo "******************************"
}

############################## ssl_policy ##############################
# local shell function to perform SSL Policy tests
########################################################################
ssl_policy()
{
  #verbose="-v"
  html_head "SSL POLICY $NORM_EXT - server $SERVER_MODE/client $CLIENT_MODE"

  testname=""
  sparam="$CIPHER_SUITES"

  if [ ! -f "${P_R_CLIENTDIR}/pkcs11.txt" ] ; then
      html_failed "${SCRIPTNAME}: ${P_R_CLIENTDIR} is not initialized"
      return 1;
  fi

  echo "Saving pkcs11.txt"
  cp ${P_R_CLIENTDIR}/pkcs11.txt ${P_R_CLIENTDIR}/pkcs11.txt.sav

  start_selfserv # Launch the server

  ignore_blank_lines ${SSLPOLICY} | \
  while read value ectype testmax param policy testname
  do
      VMIN="ssl3"

      if [ "$ectype" = "ECC" ] ; then
          echo "$SCRIPTNAME: skipping  $testname (ECC only)"
      else
          echo "$SCRIPTNAME: running $testname ----------------------------"
          VMAX="ssl3"
          if [ "$testmax" = "TLS10" ]; then
              VMAX="tls1.0"
          fi
          if [ "$testmax" = "TLS11" ]; then
              VMAX="tls1.1"
          fi
          if [ "$testmax" = "TLS12" ]; then
              VMAX="tls1.2"
          fi

          # load the policy
          policy=`echo ${policy} | sed -e 's;_; ;g'`
          setup_policy "$policy" ${P_R_CLIENTDIR}

          echo "tstclnt -4 -p ${PORT} -h ${HOSTADDR} -c ${param} -V ${VMIN}:${VMAX} ${CLIENT_OPTIONS} \\"
          echo "        -f -d ${P_R_CLIENTDIR} $verbose -w nss < ${REQUEST_FILE}"

          rm ${TMP}/$HOST.tmp.$$ 2>/dev/null
          ${PROFTOOL} ${BINDIR}/tstclnt -4 -p ${PORT} -h ${HOSTADDR} -c ${param} -V ${VMIN}:${VMAX} ${CLIENT_OPTIONS} -f \
                  -d ${P_R_CLIENTDIR} $verbose -w nss < ${REQUEST_FILE} \
                  >${TMP}/$HOST.tmp.$$  2>&1
          ret=$?
          cat ${TMP}/$HOST.tmp.$$
          rm ${TMP}/$HOST.tmp.$$ 2>/dev/null

          #workaround for bug #402058
          [ $ret -ne 0 ] && ret=1
          [ ${value} -ne 0 ] && value=1

          html_msg $ret ${value} "${testname}" \
                   "produced a returncode of $ret, expected is ${value}"
      fi
  done
  cp ${P_R_CLIENTDIR}/pkcs11.txt.sav ${P_R_CLIENTDIR}/pkcs11.txt

  kill_selfserv
  html "</TABLE><BR>"
}

list_enabled_suites()
{
  echo "SSL_DIR=${P_R_CLIENTDIR} ${BINDIR}/listsuites"
  SSL_DIR="${P_R_CLIENTDIR}" ${BINDIR}/listsuites | tail -n+3 | \
      sed -n -e '/^TLS_/h' -e '/^ .*Enabled.*/{g;p}' | sed 's/:$//'
}

############################## ssl_policy_listsuites ###################
# local shell function to perform SSL Policy tests, using listsuites
########################################################################
ssl_policy_listsuites()
{
  #verbose="-v"
  html_head "SSL POLICY LISTSUITES $NORM_EXT - server $SERVER_MODE/client $CLIENT_MODE"

  testname=""
  sparam="$CIPHER_SUITES"

  if [ ! -f "${P_R_CLIENTDIR}/pkcs11.txt" ] ; then
      html_failed "${SCRIPTNAME}: ${P_R_CLIENTDIR} is not initialized"
      return 1;
  fi

  echo "Saving pkcs11.txt"
  cp ${P_R_CLIENTDIR}/pkcs11.txt ${P_R_CLIENTDIR}/pkcs11.txt.sav

  # Disallow all explicitly
  setup_policy "disallow=all" ${P_R_CLIENTDIR}
  RET_EXP=1
  list_enabled_suites | grep '^TLS_'
  RET=$?
  html_msg $RET $RET_EXP "${testname}" \
           "produced a returncode of $RET, expected is $RET_EXP"

  # Disallow RSA in key exchange explicitly
  setup_policy "disallow=rsa/ssl-key-exchange" ${P_R_CLIENTDIR}
  RET_EXP=1
  list_enabled_suites | grep '^TLS_RSA_'
  RET=$?
  html_msg $RET $RET_EXP "${testname}" \
           "produced a returncode of $RET, expected is $RET_EXP"

  cp ${P_R_CLIENTDIR}/pkcs11.txt.sav ${P_R_CLIENTDIR}/pkcs11.txt

  html "</TABLE><BR>"
}

############################## ssl_policy_selfserv #####################
# local shell function to perform SSL Policy tests, using selfserv
########################################################################
ssl_policy_selfserv()
{
  #verbose="-v"
  html_head "SSL POLICY SELFSERV $NORM_EXT - server $SERVER_MODE/client $CLIENT_MODE"

  testname=""
  sparam="$CIPHER_SUITES"

  if [ ! -f "${P_R_SERVERDIR}/pkcs11.txt" ] ; then
      html_failed "${SCRIPTNAME}: ${P_R_SERVERDIR} is not initialized"
      return 1;
  fi

  echo "Saving pkcs11.txt"
  cp ${P_R_SERVERDIR}/pkcs11.txt ${P_R_SERVERDIR}/pkcs11.txt.sav

  # Disallow RSA in key exchange explicitly
  setup_policy "disallow=rsa/ssl-key-exchange" ${P_R_SERVERDIR}

  start_selfserv # Launch the server

  VMIN="ssl3"
  VMAX="tls1.2"

  # Try to connect to the server with a ciphersuite using RSA in key exchange
  echo "tstclnt -4 -p ${PORT} -h ${HOSTADDR} -c d -V ${VMIN}:${VMAX} ${CLIENT_OPTIONS} \\"
  echo "        -f -d ${P_R_CLIENTDIR} $verbose -w nss < ${REQUEST_FILE}"

  rm ${TMP}/$HOST.tmp.$$ 2>/dev/null
  RET_EXP=254
  ${PROFTOOL} ${BINDIR}/tstclnt -4 -p ${PORT} -h ${HOSTADDR} -c d -V ${VMIN}:${VMAX} ${CLIENT_OPTIONS} -f \
          -d ${P_R_CLIENTDIR} $verbose -w nss < ${REQUEST_FILE} \
          >${TMP}/$HOST.tmp.$$  2>&1
  RET=$?
  cat ${TMP}/$HOST.tmp.$$
  rm ${TMP}/$HOST.tmp.$$ 2>/dev/null

  html_msg $RET $RET_EXP "${testname}" \
           "produced a returncode of $RET, expected is $RET_EXP"

  cp ${P_R_SERVERDIR}/pkcs11.txt.sav ${P_R_SERVERDIR}/pkcs11.txt

  kill_selfserv
  html "</TABLE><BR>"
}

############################# is_revoked ###############################
# local shell function to check if certificate is revoked
########################################################################
is_revoked() {
    certNum=$1
    currLoadedGrp=$2

    found=0
    ownerGrp=1
    while [ $ownerGrp -le $TOTAL_GRP_NUM -a $found -eq 0 ]
      do
      currGrpBegin=`eval echo \$\{CRL_GRP_${ownerGrp}_BEGIN\}`
      currGrpRange=`eval echo \$\{CRL_GRP_${ownerGrp}_RANGE\}`
      currGrpEnd=`expr $currGrpBegin + $currGrpRange - 1`
      if [ $certNum -ge $currGrpBegin -a $certNum -le $currGrpEnd ]; then
          found=1
      else
          ownerGrp=`expr $ownerGrp + 1`
      fi
    done
    if [ $found -eq 1 -a $currLoadedGrp -lt $ownerGrp ]; then
        return 1
    fi
    if [ $found -eq 0 ]; then
        return 1
    fi
    unrevokedGrpCert=`eval echo \$\{UNREVOKED_CERT_GRP_${ownerGrp}\}`
    if [ $certNum -eq $unrevokedGrpCert ]; then
        return 1
    fi
    return 0
}

########################### load_group_crl #############################
# local shell function to load CRL
########################################################################
load_group_crl() {
    #verbose="-v"
    group=$1
    ectype=$2

    OUTFILE_TMP=${TMP}/$HOST.tmp.$$
    grpBegin=`eval echo \$\{CRL_GRP_${group}_BEGIN\}`
    grpRange=`eval echo \$\{CRL_GRP_${group}_RANGE\}`
    grpEnd=`expr $grpBegin + $grpRange - 1`

    if [ "$grpBegin" = "" -o "$grpRange" = "" ]; then
        ret=1
        return 1;
    fi

    # Add -ec suffix for ECC
    if [ "$ectype" = "ECC" ] ; then
      ecsuffix="-ec"
      eccomment="ECC "
    else
      ecsuffix=""
      eccomment=""
    fi

    if [ "$RELOAD_CRL" != "" ]; then
        if [ $group -eq 1 ]; then
            echo "==================== Resetting to group 1 crl ==================="
            kill_selfserv
            start_selfserv
            is_selfserv_alive
        fi
        echo "================= Reloading ${eccomment}CRL for group $grpBegin - $grpEnd ============="

        echo "tstclnt -4 -p ${PORT} -h ${HOSTADDR} -f -d ${R_CLIENTDIR} $verbose \\"
        echo "          -V ssl3:tls1.2 -w nss -n TestUser${UNREVOKED_CERT_GRP_1}${ecsuffix}"
        echo "Request:"
        echo "GET crl://${SERVERDIR}/root.crl_${grpBegin}-${grpEnd}${ecsuffix}"
        echo ""
        echo "RELOAD time $i"

        REQF=${R_CLIENTDIR}.crlreq
        cat > ${REQF} <<_EOF_REQUEST_
GET crl://${SERVERDIR}/root.crl_${grpBegin}-${grpEnd}${ecsuffix}

_EOF_REQUEST_

        ${PROFTOOL} ${BINDIR}/tstclnt -4 -p ${PORT} -h ${HOSTADDR} -f  \
            -d ${R_CLIENTDIR} $verbose -V ssl3:tls1.2 -w nss -n TestUser${UNREVOKED_CERT_GRP_1}${ecsuffix} \
            >${OUTFILE_TMP}  2>&1 < ${REQF}

        cat ${OUTFILE_TMP}
        grep "CRL ReCache Error" ${OUTFILE_TMP}
        if [ $? -eq 0 ]; then
            ret=1
            return 1
        fi
    else
        echo "=== Updating DB for group $grpBegin - $grpEnd and restarting selfserv ====="

        kill_selfserv
        CU_ACTION="Importing ${eccomment}CRL for groups $grpBegin - $grpEnd"
        crlu -d ${R_SERVERDIR} -I -i ${SERVERDIR}/root.crl_${grpBegin}-${grpEnd}${ecsuffix} \
             -p ../tests.pw.928
        ret=$?
        if [ "$ret" -eq 0 ]; then
	    html_passed "${CU_ACTION}"
            return 1
        fi
        start_selfserv
    fi
    is_selfserv_alive
    ret=$?
    echo "================= CRL Reloaded ============="
}


########################### ssl_crl_cache ##############################
# local shell function to perform SSL test for crl cache functionality
# with/out revoked certs
########################################################################
ssl_crl_cache()
{
  #verbose="-v"
  html_head "Cache CRL SSL Client Tests $NORM_EXT"
  SSLAUTH_TMP=${TMP}/authin.tl.tmp
  SERV_ARG=-r_-r
  rm -f ${SSLAUTH_TMP}
  echo ${SSLAUTH_TMP}

  grep -- " $SERV_ARG " ${SSLAUTH} | grep -v "^#" | grep -v none | grep -v bogus > ${SSLAUTH_TMP}
  echo $?
  while [ $? -eq 0 -a -f ${SSLAUTH_TMP} ]
    do
    sparam=$SERV_ARG
    start_selfserv
    exec < ${SSLAUTH_TMP}
    while read ectype value sparam cparam testname
      do
      [ "$ectype" = "" ] && continue
      if [ "$ectype" = "ECC" ] ; then
        echo "$SCRIPTNAME: skipping  $testname (ECC only)"
      elif [ "$ectype" = "SNI" ]; then
          continue
      else
        servarg=`echo $sparam | awk '{r=split($0,a,"-r") - 1;print r;}'`
        pwd=`echo $cparam | grep nss`
        user=`echo $cparam | grep TestUser`
        _cparam=$cparam
        case $servarg in
            1) if [ -z "$pwd" -o -z "$user" ]; then
                rev_modvalue=0
                else
                rev_modvalue=254
                fi
                ;;
            2) rev_modvalue=254 ;;

            3) if [ -z "$pwd" -o -z "$user" ]; then
                rev_modvalue=0
                else
                rev_modvalue=1
                fi
                ;;
            4) rev_modvalue=1 ;;
	  esac
        TEMP_NUM=0
        LOADED_GRP=1
        while [ ${LOADED_GRP} -le ${TOTAL_GRP_NUM} ]
          do
          while [ $TEMP_NUM -lt $TOTAL_CRL_RANGE ]
            do
            CURR_SER_NUM=`expr ${CRL_GRP_1_BEGIN} + ${TEMP_NUM}`
            TEMP_NUM=`expr $TEMP_NUM + 1`
            USER_NICKNAME="TestUser${CURR_SER_NUM}"
            cparam=`echo $_cparam | sed -e 's;_; ;g' -e "s/TestUser/$USER_NICKNAME/g" `

            echo "Server Args: $SERV_ARG"
            echo "tstclnt -4 -p ${PORT} -h ${HOSTADDR} -f -d ${R_CLIENTDIR} $verbose \\"
            echo "        ${cparam}  < ${REQUEST_FILE}"
            rm ${TMP}/$HOST.tmp.$$ 2>/dev/null
            ${PROFTOOL} ${BINDIR}/tstclnt -4 -p ${PORT} -h ${HOSTADDR} -f ${cparam} \
	        -d ${R_CLIENTDIR} $verbose < ${REQUEST_FILE} \
                >${TMP}/$HOST.tmp.$$  2>&1
            ret=$?
            cat ${TMP}/$HOST.tmp.$$
            rm ${TMP}/$HOST.tmp.$$ 2>/dev/null
            is_revoked ${CURR_SER_NUM} ${LOADED_GRP}
            isRevoked=$?
            if [ $isRevoked -eq 0 ]; then
                modvalue=$rev_modvalue
                testAddMsg="revoked"
            else
                modvalue=$value
                testAddMsg="not revoked"
            fi

            is_selfserv_alive
            ss_status=$?
            if [ "$ss_status" -ne 0 ]; then
                html_msg $ret $modvalue \
                    "${testname}(cert ${USER_NICKNAME} - $testAddMsg)" \
                    "produced a returncode of $ret, expected is $modvalue. " \
                    "selfserv is not alive!"
            else
                html_msg $ret $modvalue \
                    "${testname}(cert ${USER_NICKNAME} - $testAddMsg)" \
                    "produced a returncode of $ret, expected is $modvalue"
            fi
          done
          LOADED_GRP=`expr $LOADED_GRP + 1`
          TEMP_NUM=0
          if [ "$LOADED_GRP" -le "$TOTAL_GRP_NUM" ]; then
              load_group_crl $LOADED_GRP $ectype
              html_msg $ret 0 "Load group $LOADED_GRP ${eccomment}crl " \
                  "produced a returncode of $ret, expected is 0"
          fi
        done
        # Restart selfserv to roll back to two initial group 1 crls
        # TestCA CRL and TestCA-ec CRL
        kill_selfserv
        start_selfserv
      fi
    done
    kill_selfserv
    SERV_ARG="${SERV_ARG}_-r"
    rm -f ${SSLAUTH_TMP}
    grep -- " $SERV_ARG " ${SSLAUTH} | grep -v "^#" | grep -v none | grep -v bogus  > ${SSLAUTH_TMP}
  done
  TEMPFILES=${SSLAUTH_TMP}
  html "</TABLE><BR>"
}

############################ ssl_dtls ###################################
# local shell function to test tstclnt acting as client and server for DTLS
#########################################################################
ssl_dtls()
{
  #verbose="-v"
  html_head "SSL DTLS $NORM_EXT - server $SERVER_MODE/client $CLIENT_MODE"

    testname="ssl_dtls"
    value=0

    if [ "$SERVER_MODE" = "fips" -o "$CLIENT_MODE" = "fips" ] ; then
        echo "$SCRIPTNAME: skipping  $testname (non-FIPS only)"
        return 0
    fi

    echo "${testname}"

    echo "tstclnt -4 -p ${PORT} -h ${HOSTADDR} -f -d ${P_R_SERVERDIR} $verbose ${SERVER_OPTIONS} \\"
    echo "        -U -V tls1.1:tls1.2 -P server -Q < ${REQUEST_FILE} &"

    ${PROFTOOL} ${BINDIR}/tstclnt -4 -p ${PORT} -h ${HOSTADDR} -f ${SERVER_OPTIONS} \
                -d ${P_R_SERVERDIR} $verbose -U -V tls1.1:tls1.2 -P server -n ${HOSTADDR} -w nss < ${REQUEST_FILE} 2>&1 &

    PID=$!
    
    sleep 1
    
    echo "tstclnt -4 -p ${PORT} -h ${HOSTADDR} -f -d ${P_R_CLIENTDIR} $verbose ${CLIENT_OPTIONS} \\"
    echo "        -U -V tls1.1:tls1.2 -P client -Q < ${REQUEST_FILE}"
    ${PROFTOOL} ${BINDIR}/tstclnt -4 -p ${PORT} -h ${HOSTADDR} -f ${CLIENT_OPTIONS} \
            -d ${P_R_CLIENTDIR} $verbose -U -V tls1.1:tls1.2 -P client -Q < ${REQUEST_FILE} 2>&1 
    ret=$?
    html_msg $ret $value "${testname}" \
             "produced a returncode of $ret, expected is $value"

    kill ${PID}
    
  html "</TABLE><BR>"
}


############################## ssl_cleanup #############################
# local shell function to finish this script (no exit since it might be
# sourced)
########################################################################
ssl_cleanup()
{
  rm $SERVERPID 2>/dev/null
  cd ${QADIR}
  . common/cleanup.sh
}

############################## ssl_run #################################
# local shell function to run coverage, authentication and stress tests
########################################################################
ssl_run()
{
    for SSL_RUN in ${NSS_SSL_RUN}
    do
        case "${SSL_RUN}" in
        "stapling")
            if [ -z "$NSS_DISABLE_LIBPKIX" ]; then
              ssl_stapling
            fi
            ;;
        "signed_cert_timestamps")
            ssl_signed_cert_timestamps
            ;;
        "cov")
            ssl_cov
            ;;
        "auth")
            ssl_auth
            ;;
        "stress")
            ssl_stress
            ;;
        "dtls")
            ssl_dtls
            ;;
         esac
    done
}

############################ ssl_run_all ###############################
# local shell function to run both standard and extended ssl tests
########################################################################
ssl_run_all()
{
    ORIG_SERVERDIR=$SERVERDIR
    ORIG_CLIENTDIR=$CLIENTDIR
    ORIG_R_SERVERDIR=$R_SERVERDIR
    ORIG_R_CLIENTDIR=$R_CLIENTDIR
    ORIG_P_R_SERVERDIR=$P_R_SERVERDIR
    ORIG_P_R_CLIENTDIR=$P_R_CLIENTDIR

    # Exercise PKCS#11 URI parsing. The token actually changes its name
    # in FIPS mode, so cope with that. Note there's also semicolon in here
    # but it doesn't need escaping/quoting; the shell copes.
    if [ "${CLIENT_MODE}" = "fips" ]; then
	USER_NICKNAME="pkcs11:token=NSS%20FIPS%20140-2%20Certificate%20DB;object=TestUser"
    else
	USER_NICKNAME="pkcs11:token=NSS%20Certificate%20DB;object=TestUser"
    fi
    NORM_EXT=""
    cd ${CLIENTDIR}

    ssl_run

    SERVERDIR=$EXT_SERVERDIR
    CLIENTDIR=$EXT_CLIENTDIR
    R_SERVERDIR=$R_EXT_SERVERDIR
    R_CLIENTDIR=$R_EXT_CLIENTDIR
    P_R_SERVERDIR=$P_R_EXT_SERVERDIR
    P_R_CLIENTDIR=$P_R_EXT_CLIENTDIR

    # A different URI test; specify CKA_LABEL but not the token.
    USER_NICKNAME="pkcs11:object=ExtendedSSLUser"
    NORM_EXT="Extended Test"
    cd ${CLIENTDIR}

    ssl_run

    # the next round of ssl tests will only run if these vars are reset
    SERVERDIR=$ORIG_SERVERDIR
    CLIENTDIR=$ORIG_CLIENTDIR
    R_SERVERDIR=$ORIG_R_SERVERDIR
    R_CLIENTDIR=$ORIG_R_CLIENTDIR
    P_R_SERVERDIR=$ORIG_P_R_SERVERDIR
    P_R_CLIENTDIR=$ORIG_P_R_CLIENTDIR

    USER_NICKNAME=TestUser
    NORM_EXT=
    cd ${QADIR}/ssl
}

############################ ssl_set_fips ##############################
# local shell function to set FIPS mode on/off
########################################################################
ssl_set_fips()
{
    CLTSRV=$1
    ONOFF=$2

    if [ ${CLTSRV} = "server" ]; then
        DBDIRS="${SERVERDIR} ${EXT_SERVERDIR}"
    else
        DBDIRS="${CLIENTDIR} ${EXT_CLIENTDIR}"
    fi

    if [ "${ONOFF}" = "on" ]; then
        FIPSMODE=true
        RET_EXP=0
    else
        FIPSMODE=false
        RET_EXP=1
    fi

    html_head "SSL - FIPS mode ${ONOFF} for ${CLTSRV}"

    for DBDIR in ${DBDIRS}
    do
        EXT_OPT=
        echo ${DBDIR} | grep ext > /dev/null
        if [ $? -eq 0 ]; then
            EXT_OPT="extended "
        fi

        echo "${SCRIPTNAME}: Turning FIPS ${ONOFF} for the ${EXT_OPT} ${CLTSRV}"

        echo "modutil -dbdir ${DBDIR} -fips ${FIPSMODE} -force"
        ${BINDIR}/modutil -dbdir ${DBDIR} -fips ${FIPSMODE} -force 2>&1
        RET=$?
        html_msg "${RET}" "0" "${TESTNAME} (modutil -fips ${FIPSMODE})" \
                 "produced a returncode of ${RET}, expected is 0"

        echo "modutil -dbdir ${DBDIR} -list"
        DBLIST=`${BINDIR}/modutil -dbdir ${DBDIR} -list 2>&1`
        RET=$?
        html_msg "${RET}" "0" "${TESTNAME} (modutil -list)" \
                 "produced a returncode of ${RET}, expected is 0"

        echo "${DBLIST}" | grep "FIPS PKCS #11"
        RET=$?
        html_msg "${RET}" "${RET_EXP}" "${TESTNAME} (grep \"FIPS PKCS #11\")" \
                 "produced a returncode of ${RET}, expected is ${RET_EXP}"
    done

    html "</TABLE><BR>"
}

############################ ssl_set_fips ##############################
# local shell function to run all tests set in NSS_SSL_TESTS variable
########################################################################
ssl_run_tests()
{
    for SSL_TEST in ${NSS_SSL_TESTS}
    do
        case "${SSL_TEST}" in
        "policy")
            if [ "${TEST_MODE}" = "SHARED_DB" ] ; then
                ssl_policy_listsuites
                ssl_policy_selfserv
                ssl_policy
            fi
            ;;
        "crl")
            ssl_crl_ssl
            ssl_crl_cache
            ;;
        "iopr")
            ssl_iopr_run
            ;;
        *)
            SERVER_MODE=`echo "${SSL_TEST}" | cut -d_ -f1`
            CLIENT_MODE=`echo "${SSL_TEST}" | cut -d_ -f2`

            case "${SERVER_MODE}" in
            "normal")
                SERVER_OPTIONS=
                ;;
            "fips")
                SERVER_OPTIONS=
                ssl_set_fips server on
                ;;
            *)
                html_failed "${SCRIPTNAME}: Error: Unknown server mode ${SERVER_MODE}"
                return 1
                ;;
            esac

            case "${CLIENT_MODE}" in
            "normal")
                CLIENT_OPTIONS=
                ;;
            "fips")
                SERVER_OPTIONS=
                ssl_set_fips client on
                ;;
            *)
                html_failed "${SCRIPTNAME}: Error: Unknown client mode ${CLIENT_MODE}"
                return 1
                ;;
            esac

            ssl_run_all

            if [ "${SERVER_MODE}" = "fips" ]; then
                ssl_set_fips server off
            fi

            if [ "${CLIENT_MODE}" = "fips" ]; then
                ssl_set_fips client off
            fi
            ;;
        esac
    done
}

################################# main #################################

ssl_init
ssl_run_tests
ssl_cleanup

