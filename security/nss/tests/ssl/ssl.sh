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
#   Slavomir Katuscak <slavomir.katuscak@sun.com>, Sun Microsystems
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

  # Test case files
  SSLCOV=${QADIR}/ssl/sslcov.txt
  SSLAUTH=${QADIR}/ssl/sslauth.txt
  SSLSTRESS=${QADIR}/ssl/sslstress.txt
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

  if [ -n "$NSS_ENABLE_ECC" ] ; then
      ECC_STRING=" - with ECC"
  else
      ECC_STRING=""
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
  if [ "${OS_ARCH}" = "WINNT" -a "$OS_NAME" = "CYGWIN_NT" ]; then
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
  echo "waiting for selfserv at `date`"
  echo "tstclnt -p ${PORT} -h ${HOSTADDR} ${CLIENT_OPTIONS} -q \\"
  echo "        -d ${P_R_CLIENTDIR} < ${REQUEST_FILE}"
  tstclnt -p ${PORT} -h ${HOSTADDR} ${CLIENT_OPTIONS} -q \
          -d ${P_R_CLIENTDIR} < ${REQUEST_FILE}
  if [ $? -ne 0 ]; then
      html_failed "<TR><TD> Wait for Server "
      echo "RETRY: tstclnt -p ${PORT} -h ${HOSTADDR} ${CLIENT_OPTIONS} -q \\"
      echo "               -d ${P_R_CLIENTDIR} < ${REQUEST_FILE}"
      tstclnt -p ${PORT} -h ${HOSTADDR} ${CLIENT_OPTIONS} -q \
              -d ${P_R_CLIENTDIR} < ${REQUEST_FILE}
  elif [ "$sparam" = "$CSHORT" -o "$sparam" = "$CLONG" ] ; then
      html_passed "<TR><TD> Wait for Server"
  fi
  is_selfserv_alive
}

########################### kill_selfserv ##############################
# local shell function to kill the selfserver after the tests are done
########################################################################
kill_selfserv()
{
  if [ "${OS_ARCH}" = "WINNT" -a "$OS_NAME" = "CYGWIN_NT" ]; then
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
      until selfserv -b -p ${PORT} 2>/dev/null; do
          echo "RETRY: selfserv -b -p ${PORT} 2>/dev/null;"
          sleep 1
      done
  fi

  echo "selfserv with PID ${PID} killed at `date`"

  rm ${SERVERPID}
  html_detect_core "<TR><TD>kill_selfserv core detection step"
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
  if [ -n "$NSS_ENABLE_ECC" ] && \
     [ -z "$NO_ECC_CERTS" -o "$NO_ECC_CERTS" != "1"  ] ; then
      ECC_OPTIONS="-e ${HOSTADDR}-ec"
  else
      ECC_OPTIONS=""
  fi
  if [ "$1" = "mixed" ]; then
      ECC_OPTIONS="-e ${HOSTADDR}-ecmixed"
  fi
  echo "selfserv starting at `date`"
  echo "selfserv -D -p ${PORT} -d ${P_R_SERVERDIR} -n ${HOSTADDR} ${SERVER_OPTIONS} \\"
  echo "         ${ECC_OPTIONS} -w nss ${sparam} -i ${R_SERVERPID} $verbose &"
  if [ ${fileout} -eq 1 ]; then
      selfserv -D -p ${PORT} -d ${P_R_SERVERDIR} -n ${HOSTADDR} ${SERVER_OPTIONS} \
               ${ECC_OPTIONS} -w nss ${sparam} -i ${R_SERVERPID} $verbose \
               > ${SERVEROUTFILE} 2>&1 &
  else
      selfserv -D -p ${PORT} -d ${P_R_SERVERDIR} -n ${HOSTADDR} ${SERVER_OPTIONS} \
               ${ECC_OPTIONS} -w nss ${sparam} -i ${R_SERVERPID} $verbose &
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

  if [ "${OS_ARCH}" = "WINNT" -a "$OS_NAME" = "CYGWIN_NT" ]; then
      PID=${SHELL_SERVERPID}
  else
      PID=`cat ${SERVERPID}`
  fi

  echo "selfserv with PID ${PID} started at `date`"
}

############################## ssl_cov #################################
# local shell function to perform SSL Cipher Coverage tests
########################################################################
ssl_cov()
{
  html_head "SSL Cipher Coverage $NORM_EXT - $BYPASS_STRING $ECC_STRING"

  testname=""
  if [ -n "$NSS_ENABLE_ECC" ] ; then
      sparam="$CLONG"
  else
      sparam="$CSHORT"
  fi

  mixed=0
  start_selfserv # Launch the server
               
  p=""

  while read ectype tls param testname
  do
      p=`echo "$testname" | sed -e "s/_.*//"`   #sonmi, only run extended test on SSL3 and TLS
      
      if [ "$p" = "SSL2" -a "$NORM_EXT" = "Extended Test" ] ; then
          echo "$SCRIPTNAME: skipping  $testname for $NORM_EXT"
      elif [ "$ectype" = "ECC" -a  -z "$NSS_ENABLE_ECC" ] ; then
          echo "$SCRIPTNAME: skipping  $testname (ECC only)"
      elif [ "$ectype" != "#" ] ; then
          echo "$SCRIPTNAME: running $testname ----------------------------"
          TLS_FLAG=-T
          if [ "$tls" = "TLS" ]; then
              TLS_FLAG=""
          fi

# These five tests need an EC cert signed with RSA
# This requires a different certificate loaded in selfserv
# due to a (current) NSS limitation of only loaded one cert
# per type so the default selfserv setup will not work.
#:C00B TLS ECDH RSA WITH NULL SHA
#:C00C TLS ECDH RSA WITH RC4 128 SHA
#:C00D TLS ECDH RSA WITH 3DES EDE CBC SHA
#:C00E TLS ECDH RSA WITH AES 128 CBC SHA
#:C00F TLS ECDH RSA WITH AES 256 CBC SHA

          if [ $mixed -eq 0 ]; then
            if [ "${param}" = ":C00B" -o "${param}" = ":C00C" -o "${param}" = ":C00D" -o "${param}" = ":C00E" -o "${param}" = ":C00F" ]; then
              kill_selfserv
              start_selfserv mixed
              mixed=1
            else
              is_selfserv_alive
            fi
          else 
            if [ "${param}" = ":C00B" -o "${param}" = ":C00C" -o "${param}" = ":C00D" -o "${param}" = ":C00E" -o "${param}" = ":C00F" ]; then
              is_selfserv_alive
            else
              kill_selfserv
              start_selfserv
              mixed=0
            fi
          fi

          echo "tstclnt -p ${PORT} -h ${HOSTADDR} -c ${param} ${TLS_FLAG} ${CLIENT_OPTIONS} \\"
          echo "        -f -d ${P_R_CLIENTDIR} < ${REQUEST_FILE}"

          rm ${TMP}/$HOST.tmp.$$ 2>/dev/null
          tstclnt -p ${PORT} -h ${HOSTADDR} -c ${param} ${TLS_FLAG} ${CLIENT_OPTIONS} -f \
                  -d ${P_R_CLIENTDIR} < ${REQUEST_FILE} \
                  >${TMP}/$HOST.tmp.$$  2>&1
          ret=$?
          cat ${TMP}/$HOST.tmp.$$ 
          rm ${TMP}/$HOST.tmp.$$ 2>/dev/null
          html_msg $ret 0 "${testname}" \
                   "produced a returncode of $ret, expected is 0"
      fi
  done < ${SSLCOV}

  kill_selfserv
  html "</TABLE><BR>"
}

############################## ssl_auth ################################
# local shell function to perform SSL  Client Authentication tests
########################################################################
ssl_auth()
{
  html_head "SSL Client Authentication $NORM_EXT - $BYPASS_STRING $ECC_STRING"

  while read ectype value sparam cparam testname
  do
      if [ "$ectype" = "ECC" -a  -z "$NSS_ENABLE_ECC" ] ; then
          echo "$SCRIPTNAME: skipping  $testname (ECC only)"
      elif [ "$ectype" != "#" ]; then
          cparam=`echo $cparam | sed -e 's;_; ;g' -e "s/TestUser/$USER_NICKNAME/g" `
          start_selfserv

          echo "tstclnt -p ${PORT} -h ${HOSTADDR} -f -d ${P_R_CLIENTDIR} ${CLIENT_OPTIONS} \\"
	  echo "        ${cparam}  < ${REQUEST_FILE}"
          rm ${TMP}/$HOST.tmp.$$ 2>/dev/null
          tstclnt -p ${PORT} -h ${HOSTADDR} -f ${cparam} ${CLIENT_OPTIONS} \
                  -d ${P_R_CLIENTDIR} < ${REQUEST_FILE} \
                  >${TMP}/$HOST.tmp.$$  2>&1
          ret=$?
          cat ${TMP}/$HOST.tmp.$$ 
          rm ${TMP}/$HOST.tmp.$$ 2>/dev/null

          html_msg $ret $value "${testname}" \
                   "produced a returncode of $ret, expected is $value"
          kill_selfserv
      fi
  done < ${SSLAUTH}

  html "</TABLE><BR>"
}


############################## ssl_stress ##############################
# local shell function to perform SSL stress test
########################################################################
ssl_stress()
{
  html_head "SSL Stress Test $NORM_EXT - $BYPASS_STRING $ECC_STRING"

  while read ectype value sparam cparam testname
  do
      if [ -z "$ectype" ]; then
          # silently ignore blank lines
          continue
      fi
      p=`echo "$testname" | sed -e "s/Stress //" -e "s/ .*//"`   #sonmi, only run extended test on SSL3 and TLS
      if [ "$p" = "SSL2" -a "$NORM_EXT" = "Extended Test" ] ; then
          echo "$SCRIPTNAME: skipping  $testname for $NORM_EXT"
      elif [ "$ectype" = "ECC" -a  -z "$NSS_ENABLE_ECC" ] ; then
          echo "$SCRIPTNAME: skipping  $testname (ECC only)"
      elif [ "$ectype" != "#" ]; then
          cparam=`echo $cparam | sed -e 's;_; ;g' -e "s/TestUser/$USER_NICKNAME/g" `

# These tests need the mixed cert 
# Stress TLS ECDH-RSA AES 128 CBC with SHA (no reuse)
# Stress TLS ECDH-RSA AES 128 CBC with SHA (no reuse, client auth)
          p=`echo "$sparam" | sed -e "s/\(.*\)\(-c_:C0..\)\(.*\)/\2/"`;
          if [ "$p" = "-c_:C00E" ]; then
              start_selfserv mixed
          else
              start_selfserv
          fi

          if [ "`uname -n`" = "sjsu" ] ; then
              echo "debugging disapering selfserv... ps -ef | grep selfserv"
              ps -ef | grep selfserv
          fi

          echo "strsclnt -q -p ${PORT} -d ${P_R_CLIENTDIR} ${CLIENT_OPTIONS} -w nss $cparam \\"
          echo "         $verbose ${HOSTADDR}"
          echo "strsclnt started at `date`"
          strsclnt -q -p ${PORT} -d ${P_R_CLIENTDIR} ${CLIENT_OPTIONS} -w nss $cparam \
                   $verbose ${HOSTADDR}
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
  done < ${SSLSTRESS}

  html "</TABLE><BR>"
}

############################## ssl_crl #################################
# local shell function to perform SSL test with/out revoked certs tests
########################################################################

ssl_crl_ssl()
{
  html_head "CRL SSL Client Tests $NORM_EXT $ECC_STRING"
  
  # Using First CRL Group for this test. There are $CRL_GRP_1_RANGE certs in it.
  # Cert number $UNREVOKED_CERT_GRP_1 was not revoked
  CRL_GROUP_BEGIN=$CRL_GRP_1_BEGIN
  CRL_GROUP_RANGE=$CRL_GRP_1_RANGE
  UNREVOKED_CERT=$UNREVOKED_CERT_GRP_1

  while read ectype value sparam cparam testname
  do
    if [ "$ectype" = "ECC" -a  -z "$NSS_ENABLE_ECC" ] ; then
        echo "$SCRIPTNAME: skipping $testname (ECC only)"
    elif [ "$ectype" != "#" ]; then
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
	  
	  echo "tstclnt -p ${PORT} -h ${HOSTADDR} -f -d ${R_CLIENTDIR} \\"
	  echo "        ${cparam}  < ${REQUEST_FILE}"
	  rm ${TMP}/$HOST.tmp.$$ 2>/dev/null
	  tstclnt -p ${PORT} -h ${HOSTADDR} -f ${cparam} \
	      -d ${R_CLIENTDIR} < ${REQUEST_FILE} \
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
  done < ${SSLAUTH}

  html "</TABLE><BR>"
}

############################## ssl_crl #################################
# local shell function to perform SSL test for crl cache functionality
# with/out revoked certs 
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

load_group_crl() {
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

        echo "tstclnt -p ${PORT} -h ${HOSTADDR} -f -d ${R_CLIENTDIR} \\"
        echo "          -w nss -n TestUser${UNREVOKED_CERT_GRP_1}${ecsuffix}"
        echo "Request:"
        echo "GET crl://${SERVERDIR}/root.crl_${grpBegin}-${grpEnd}${ecsuffix}"
        echo ""
        echo "RELOAD time $i"
        tstclnt -p ${PORT} -h ${HOSTADDR} -f  \
            -d ${R_CLIENTDIR} -w nss -n TestUser${UNREVOKED_CERT_GRP_1}${ecsuffix} \
	    >${OUTFILE_TMP}  2>&1 <<_EOF_REQUEST_
GET crl://${SERVERDIR}/root.crl_${grpBegin}-${grpEnd}${ecsuffix}

_EOF_REQUEST_
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
	    html_passed "<TR><TD> ${CU_ACTION}"
            return 1
        fi
        start_selfserv        
    fi
    is_selfserv_alive
    ret=$?
    echo "================= CRL Reloaded ============="
}


ssl_crl_cache()
{
  html_head "Cache CRL SSL Client Tests $NORM_EXT $ECC_STRING"
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
    while read ectype value sparam cparam testname
      do
      if [ "$ectype" = "ECC" -a  -z "$NSS_ENABLE_ECC" ] ; then
        echo "$SCRIPTNAME: skipping  $testname (ECC only)"
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
            echo "tstclnt -p ${PORT} -h ${HOSTADDR} -f -d ${R_CLIENTDIR} \\"
            echo "        ${cparam}  < ${REQUEST_FILE}"
            rm ${TMP}/$HOST.tmp.$$ 2>/dev/null
            tstclnt -p ${PORT} -h ${HOSTADDR} -f ${cparam} \
	        -d ${R_CLIENTDIR} < ${REQUEST_FILE} \
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
    done < ${SSLAUTH_TMP}
    kill_selfserv
    SERV_ARG="${SERV_ARG}_-r"
    rm -f ${SSLAUTH_TMP}
    grep -- " $SERV_ARG " ${SSLAUTH} | grep -v none | grep -v bogus  > ${SSLAUTH_TMP}
  done
  TEMPFILES=${SSLAUTH_TMP}
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


############################## ssl_run ### #############################
# local shell function to run both standard and extended ssl tests
########################################################################
ssl_run()
{
    ssl_init

    ssl_cov
    ssl_auth
    ssl_stress

    SERVERDIR=$EXT_SERVERDIR
    CLIENTDIR=$EXT_CLIENTDIR
    R_SERVERDIR=$R_EXT_SERVERDIR
    R_CLIENTDIR=$R_EXT_CLIENTDIR
    P_R_SERVERDIR=$P_R_EXT_SERVERDIR
    P_R_CLIENTDIR=$P_R_EXT_CLIENTDIR
    USER_NICKNAME=ExtendedSSLUser
    NORM_EXT="Extended Test"
    cd ${CLIENTDIR}
    ssl_cov
    ssl_auth
    ssl_stress

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

################## main #################################################

#this script may be sourced from the distributed stress test - in this case do nothing...

CSHORT="-c ABCDEF:0041:0084cdefgijklmnvyz"
CLONG="-c ABCDEF:C001:C002:C003:C004:C005:C006:C007:C008:C009:C00A:C00B:C00C:C00D:C00E:C00F:C010:C011:C012:C013:C014:0041:0084cdefgijklmnvyz"


if [ -z  "$DO_REM_ST" -a -z  "$DO_DIST_ST" ] ; then

    ssl_init

    # save the directories as setup by init.sh
    ORIG_SERVERDIR=$SERVERDIR
    ORIG_CLIENTDIR=$CLIENTDIR
    ORIG_R_SERVERDIR=$R_SERVERDIR
    ORIG_R_CLIENTDIR=$R_CLIENTDIR
    ORIG_P_R_SERVERDIR=$P_R_SERVERDIR
    ORIG_P_R_CLIENTDIR=$P_R_CLIENTDIR

    if [ -z "$NSS_TEST_DISABLE_CRL" ] ; then
        ssl_crl_ssl
        ssl_crl_cache
    else
        echo "$SCRIPTNAME: Skipping CRL Client Tests"
    fi

    # Test all combinations of client bypass and server bypass

    if [ -z "$NSS_TEST_DISABLE_CIPHERS" ] ; then
	if [ -n "$NSS_TEST_DISABLE_BYPASS" ] ; then
            SERVER_OPTIONS=""
            CLIENT_OPTIONS=""
            BYPASS_STRING="No Bypass"
            ssl_run
	fi

        if [ -z "$NSS_TEST_DISABLE_BYPASS" -a -z "$NSS_TEST_DISABLE_CLIENT_BYPASS" ] ; then
            CLIENT_OPTIONS="-B -s"
            SERVER_OPTIONS=""
            BYPASS_STRING="Client Bypass"
            ssl_run
        else
            echo "$SCRIPTNAME: Skipping Cipher Coverage - Client Bypass Tests"
        fi

        if [ -z "$NSS_TEST_DISABLE_BYPASS" -a -z "$NSS_TEST_DISABLE_SERVER_BYPASS" ] ; then
            SERVER_OPTIONS="-B -s"
            CLIENT_OPTIONS=""
            BYPASS_STRING="Server Bypass"
            ssl_run
        else
            echo "$SCRIPTNAME: Skipping Cipher Coverage - Server Bypass Tests"
        fi
    else
        echo "$SCRIPTNAME: Skipping Cipher Coverage Tests"
    fi

    ssl_iopr_run
    ssl_cleanup
fi
