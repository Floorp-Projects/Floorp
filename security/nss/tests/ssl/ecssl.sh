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
# mozilla/security/nss/tests/ssl/ecssl.sh
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
  if [ ! -r $CERT_LOG_FILE ]; then  # we need certificates here
      cd ../cert
      . ./cert.sh
  fi
  SCRIPTNAME=ssl.sh
  echo "$SCRIPTNAME: SSL tests ==============================="

  grep "SUCCESS: SSL passed" $CERT_LOG_FILE >/dev/null || {
      html_head "SSL Test failure"
      Exit 8 "Fatal - SSL of cert.sh needs to pass first"
  }

  PORT=${PORT-8443}

  # Test case files
  SSLCOV=${QADIR}/ssl/sslcov.txt
  SSLAUTH=${QADIR}/ssl/sslauth.txt
  SSLSTRESS=${QADIR}/ssl/sslstress.txt
  REQUEST_FILE=${QADIR}/ssl/sslreq.txt

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
  PID=`cat ${SERVERPID}`
  #if  [ "${OS_ARCH}" = "Linux" ]; then
      kill -0 $PID >/dev/null 2>/dev/null || Exit 10 "Fatal - selfserv process not detectable"
  #else
      #$PS -e | grep $PID >/dev/null || \
          #Exit 10 "Fatal - selfserv process not detectable"
  #fi
}

########################### wait_for_selfserv ##########################
# local shell function to wait until selfserver is running and initialized
########################################################################
wait_for_selfserv()
{
  echo "tstclnt -p ${PORT} -h ${HOSTADDR} -q "
  echo "        -d ${P_R_CLIENTDIR} < ${REQUEST_FILE} \\"
  #echo "tstclnt -q started at `date`"
  tstclnt -p ${PORT} -h ${HOSTADDR} -q -d ${P_R_CLIENTDIR} < ${REQUEST_FILE}
  if [ $? -ne 0 ]; then
      html_failed "<TR><TD> Wait for Server "
      echo "RETRY: tstclnt -p ${PORT} -h ${HOSTADDR} -q \\"
      echo "               -d ${P_R_CLIENTDIR} < ${REQUEST_FILE}"
      tstclnt -p ${PORT} -h ${HOSTADDR} -q -d ${P_R_CLIENTDIR} < ${REQUEST_FILE}
  elif [ sparam = "-c ABCDEFGHIJKLMNOPQRSTabcdefghijklmnvy" ] ; then # "$1" = "cov" ] ; then
      html_passed "<TR><TD> Wait for Server"
  fi
  is_selfserv_alive
}

########################### kill_selfserv ##############################
# local shell function to kill the selfserver after the tests are done
########################################################################
kill_selfserv()
{
  ${KILL} `cat ${SERVERPID}`
  wait `cat ${SERVERPID}`
  if [ ${fileout} -eq 1 ]; then
      cat ${SERVEROUTFILE}
  fi
  # On Linux selfserv needs up to 30 seconds to fully die and free
  # the port.  Wait until the port is free. (Bug 129701)
  if [ "${OS_ARCH}" = "Linux" ]; then
      until selfserv -b -p ${PORT} 2>/dev/null; do
          sleep 1
      done
  fi
  rm ${SERVERPID}
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
  echo "selfserv -D -p ${PORT} -d ${P_R_SERVERDIR} -n ${HOSTADDR} \\"
  echo "         -e ${HOSTADDR}-ec \\"
  echo "         -w nss ${sparam} -i ${R_SERVERPID} $verbose &"
  echo "selfserv started at `date`"
  if [ ${fileout} -eq 1 ]; then
      selfserv -D -p ${PORT} -d ${P_R_SERVERDIR} -n ${HOSTADDR} \
	       -e ${HOSTADDR}-ec \
               -w nss ${sparam} -i ${R_SERVERPID} $verbose \
               > ${SERVEROUTFILE} 2>&1 &
  else
      selfserv -D -p ${PORT} -d ${P_R_SERVERDIR} -n ${HOSTADDR} \
	       -e ${HOSTADDR}-ec \
               -w nss ${sparam} -i ${R_SERVERPID} $verbose &
  fi
  wait_for_selfserv
}

############################## ssl_cov #################################
# local shell function to perform SSL Cipher Coverage tests
########################################################################
ssl_cov()
{
  html_head "SSL Cipher Coverage $NORM_EXT"

  testname=""
  sparam="-c ABCDEFGHIJKLMNOPQRSTabcdefghijklmnvyz"
  start_selfserv # Launch the server
               
  p=""

  while read tls param testname
  do
      p=`echo "$testname" | sed -e "s/ .*//"`   #sonmi, only run extended test on SSL3 and TLS
      
      if [ "$p" = "SSL2" -a "$NORM_EXT" = "Extended test" ] ; then
          echo "$SCRIPTNAME: skipping  $testname for $NORM_EXT"
      elif [ "$tls" != "#" ] ; then
          echo "$SCRIPTNAME: running $testname ----------------------------"
          TLS_FLAG=-T
          if [ $tls = "TLS" ]; then
              TLS_FLAG=""
          fi

          is_selfserv_alive
          echo "tstclnt -p ${PORT} -h ${HOSTADDR} -c ${param} ${TLS_FLAG} \\"
          echo "        -f -d ${P_R_CLIENTDIR} < ${REQUEST_FILE}"

          rm ${TMP}/$HOST.tmp.$$ 2>/dev/null
          tstclnt -p ${PORT} -h ${HOSTADDR} -c ${param} ${TLS_FLAG} -f \
                  -d ${P_R_CLIENTDIR} < ${REQUEST_FILE} \
                  >${TMP}/$HOST.tmp.$$  2>&1
          ret=$?
          cat ${TMP}/$HOST.tmp.$$ 
          rm ${TMP}/$HOST.tmp.$$ 2>/dev/null
          html_msg $ret 0 "${testname}"
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
  html_head "SSL Client Authentication $NORM_EXT"

  while read value sparam cparam testname
  do
      if [ $value != "#" ]; then
          cparam=`echo $cparam | sed -e 's;_; ;g' -e "s/TestUser/$USER_NICKNAME/g" `
          start_selfserv

          echo "tstclnt -p ${PORT} -h ${HOSTADDR} -f -d ${P_R_CLIENTDIR} \\"
	  echo "        ${cparam}  < ${REQUEST_FILE}"
          rm ${TMP}/$HOST.tmp.$$ 2>/dev/null
          tstclnt -p ${PORT} -h ${HOSTADDR} -f ${cparam} \
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
  html_head "SSL Stress Test $NORM_EXT"

  while read value sparam cparam testname
  do
      p=`echo "$testname" | sed -e "s/Stress //" -e "s/ .*//"`   #sonmi, only run extended test on SSL3 and TLS
      if [ "$p" = "SSL2" -a "$NORM_EXT" = "Extended test" ] ; then
          echo "$SCRIPTNAME: skipping  $testname for $NORM_EXT"
      elif [ $value != "#" ]; then
          cparam=`echo $cparam | sed -e 's;_; ;g'`
          start_selfserv
          if [ `uname -n` = "sjsu" ] ; then
              echo "debugging disapering selfserv... ps -ef | grep selfserv"
              ps -ef | grep selfserv
          fi

          echo "strsclnt -q -p ${PORT} -d ${P_R_CLIENTDIR} -w nss $cparam \\"
          echo "         $verbose ${HOSTADDR}"
          echo "strsclnt started at `date`"
          strsclnt -q -p ${PORT} -d ${P_R_CLIENTDIR} -w nss $cparam \
                   $verbose ${HOSTADDR}
          ret=$?
          echo "strsclnt completed at `date`"
          html_msg $ret $value "${testname}"
          if [ `uname -n` = "sjsu" ] ; then
              echo "debugging disapering selfserv... ps -ef | grep selfserv"
              ps -ef | grep selfserv
          fi
          kill_selfserv
      fi
  done < ${SSLSTRESS}

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

################## main #################################################

#this script may be sourced from the distributed stress test - in this case do nothing...

if [ -z  "$DO_REM_ST" -a -z  "$DO_DIST_ST" ] ; then
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
    NORM_EXT="Extended test"
    cd ${CLIENTDIR}
    ssl_cov
    ssl_auth
    ssl_stress
    ssl_cleanup
fi
