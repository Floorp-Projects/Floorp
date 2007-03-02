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
# mozilla/security/nss/tests/iopr/ssl_iopr.sh
#
# NSS SSL interoperability QA. This file is included from ssl.sh
#
# needs to work on all Unix and Windows platforms
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
# FIXME - Netscape - NSS
########################################################################
IOPR_SSL_SOURCED=1

########################################################################
# The functions works with variables defined in interoperability 
# configuration file that was downloaded from a webserver.
# It tries to find unrevoked cert based on value of variable
# "SslClntValidCertName" defined in the configuration file.
# Params NONE.
# Returns 0 if found, 1 otherwise.
#
setValidCert() {
    testUser=$SslClntValidCertName
    [ -z "$testUser" ] && return 1
    return 0
}

########################################################################
# The funtions works with variables defined in interoperability 
# configuration file that was downloaded from a webserver.
# The function sets port, url, param and description test parameters
# that was defind for a particular type of testing.
# Params:
#      $1 - supported types of testing. Currently have maximum
#           of two: forward and reverse. But more can be defined. 
# No return value
#
setTestParam() {
    type=$1
    sslPort=`eval 'echo $'${type}Port`
    sslUrl=`eval 'echo $'${type}Url`
    testParam=`eval 'echo $'${type}Param`
    testDescription=`eval 'echo $'${type}Descr`
    [ -z "$sslPort" ] && sslPort=443
    [ -z "$sslUrl" ] && sslUrl="/iopr_test/test_pg.html"
    [ "$sslUrl" = "/" ] && sslUrl="/test_pg.html"
}


#######################################################################
# local shell function to perform SSL Cipher Suite Coverage tests
# in interoperability mode. Tests run against web server by using nss
# test client
# Params:
#      $1 - supported type of testing.
#      $2 - testing host
#      $3 - nss db location
# No return value
#  
ssl_iopr_cov_ext_server()
{
  testType=$1
  host=$2
  dbDir=$3

  setTestParam $testType
  if [ "`echo $testParam | grep NOCOV`" != "" ]; then
      echo "SSL Cipher Coverage of WebServ($IOPR_HOSTADDR) excluded from " \
           "run by server configuration"
      return 0
  fi

  html_head "SSL Cipher Coverage of WebServ($IOPR_HOSTADDR" \
      "$BYPASS_STRING $NORM_EXT): $testDescription"

  setValidCert; ret=$?
  if [ $ret -ne 0 ]; then
      html_failed "<TR><TD>Fail to find valid test cert(ws: $host)" 
      return $ret
  fi

  SSL_REQ_FILE=${TMP}/sslreq.dat.$$
  echo "GET $sslUrl HTTP/1.0" > $SSL_REQ_FILE
  echo >> $SSL_REQ_FILE
  
  while read ecc tls param testname therest; do
      [ -z "$ecc" -o "$ecc" = "#" -o "`echo $testname | grep FIPS`" -o \
          "$ecc" = "ECC" ] && continue; 
      
      echo "$SCRIPTNAME: running $testname ----------------------------"
      TLS_FLAG=-T
      if [ "$tls" = "TLS" ]; then
          TLS_FLAG=""
      fi
      
      resFile=${TMP}/$HOST.tmpRes.$$
      rm $resFile 2>/dev/null
      
      echo "tstclnt -p ${sslPort} -h ${host} -c ${param} ${TLS_FLAG} \\"
      echo "      -n $testUser -w nss ${CLIEN_OPTIONS} -f \\"
      echo "      -d ${dbDir} < ${SSL_REQ_FILE} > $resFile"
      
      tstclnt -w nss -p ${sslPort} -h ${host} -c ${param} \
          ${TLS_FLAG} ${CLIEN_OPTIONS} -f -n $testUser -w nss \
          -d ${dbDir} < ${SSL_REQ_FILE} >$resFile  2>&1
      ret=$?
      grep "ACCESS=OK" $resFile
      test $? -eq 0 -a $ret -eq 0
      ret=$?
      [ $ret -ne 0 ] && cat ${TMP}/$HOST.tmp.$$
      rm -f $resFile 2>/dev/null
      html_msg $ret 0 "${testname}"
  done < ${SSLCOV}
  rm -f $SSL_REQ_FILE 2>/dev/null

  html "</TABLE><BR>"
}

#######################################################################
# local shell function to perform SSL  Client Authentication tests
# in interoperability mode. Tests run against web server by using nss
# test client
# Params:
#      $1 - supported type of testing.
#      $2 - testing host
#      $3 - nss db location
# No return value
#  
ssl_iopr_auth_ext_server()
{
  testType=$1
  host=$2
  dbDir=$3

  setTestParam $testType
  if [ "`echo $testParam | grep NOAUTH`" != "" ]; then
      echo "SSL Client Authentication WebServ($IOPR_HOSTADDR) excluded from " \
           "run by server configuration"
      return 0
  fi

  html_head "SSL Client Authentication WebServ($IOPR_HOSTADDR $BYPASS_STRING $NORM_EXT):
             $testDescription"

  setValidCert;ret=$?
  if [ $ret -ne 0 ]; then
      html_failed "<TR><TD>Fail to find valid test cert(ws: $host)" 
      return $ret
  fi

  SSL_REQ_FILE=${TMP}/sslreq.dat.$$
  echo "GET $sslUrl HTTP/1.0" > $SSL_REQ_FILE
  echo >> $SSL_REQ_FILE
  
  SSLAUTH_TMP=${TMP}/authin.tl.tmp
  grep -v "^#" ${SSLAUTH} | grep -- "-r_-r_-r_-r" > ${SSLAUTH_TMP}

  while read ecc value sparam cparam testname; do
      [ -z "$ecc" -o "$ecc" = "#" -o "$ecc" = "ECC" ] && continue;

      cparam=`echo $cparam | sed -e 's;_; ;g' -e "s/TestUser/$testUser/g" `
      
      echo "tstclnt -p ${sslPort} -h ${host} ${CLIEN_OPTIONS} -f ${cparam} \\"
      echo "         -d ${dbDir} < ${SSL_REQ_FILE}"
      
      resFile=${TMP}/$HOST.tmp.$$
      rm $rsFile 2>/dev/null

      tstclnt -p ${sslPort} -h ${host} ${CLIEN_OPTIONS} -f ${cparam} \
          -d ${dbDir} < ${SSL_REQ_FILE} >$resFile  2>&1
      ret=$?
      grep "ACCESS=OK" $resFile
      test $? -eq 0 -a $ret -eq 0
      ret=$?
      [ $ret -ne 0 ] && cat $resFile
      rm $resFile 2>/dev/null
      
      html_msg $ret $value "${testname}. Client params: $cparam"\
          "produced a returncode of $ret, expected is $value"
  done < ${SSLAUTH_TMP}
  rm -f ${SSLAUTH_TMP} ${SSL_REQ_FILE}

  html "</TABLE><BR>"
}

########################################################################
# local shell function to perform SSL interoperability test with/out
# revoked certs tests. Tests run against web server by using nss
# test client
# Params:
#      $1 - supported type of testing.
#      $2 - testing host
#      $3 - nss db location
# No return value
#  
ssl_iopr_crl_ext_server()
{
  testType=$1
  host=$2
  dbDir=$3

  setTestParam $testType
  if [ "`echo $testParam | grep NOCRL`" != "" ]; then
      echo "CRL SSL Client Tests of WebServerv($IOPR_HOSTADDR) excluded from " \
           "run by server configuration"
      return 0
  fi

  html_head "CRL SSL Client Tests of WebServer($IOPR_HOSTADDR $BYPASS_STRING $NORM_EXT): $testDescription"
  
  SSL_REQ_FILE=${TMP}/sslreq.dat.$$
  echo "GET $sslUrl HTTP/1.0" > $SSL_REQ_FILE
  echo >> $SSL_REQ_FILE
  
  SSLAUTH_TMP=${TMP}/authin.tl.tmp
  grep -v "^#" ${SSLAUTH} | grep -- "-r_-r_-r_-r" | grep -v bogus | \
      grep -v none > ${SSLAUTH_TMP}

  while read ecc value sparam _cparam testname; do
      [ -z "$ecc" -o "$ecc" = "#" -o "$ecc" = "ECC" ] && continue;

      rev_modvalue=254
      for testUser in $SslClntValidCertName $SslClntRevokedCertName; do
          cparam=`echo $_cparam | sed -e 's;_; ;g' -e "s/TestUser/$testUser/g" `
	  
          echo "tstclnt -p ${sslPort} -h ${host} ${CLIEN_OPTIONS} \\"
          echo "        -f -d ${dbDir} ${cparam}  < ${SSL_REQ_FILE}"
          resFile=${TMP}/$HOST.tmp.$$
          rm -f $resFile 2>/dev/null
          tstclnt -p ${sslPort} -h ${host} ${CLIEN_OPTIONS} -f ${cparam} \
              -d ${dbDir} < ${SSL_REQ_FILE} \
              > $resFile  2>&1
          ret=$?
          grep "ACCESS=OK" $resFile
          test $? -eq 0 -a $ret -eq 0
          ret=$?
          [ $ret -ne 0 ] && ret=$rev_modvalue;
          [ $ret -ne 0 ] && cat $resFile
          rm -f $resFile 2>/dev/null

          if [ "`echo $SslClntRevokedCertName | grep $testUser`" != "" ]; then
              modvalue=$rev_modvalue
              testAddMsg="revoked"
          else
              testAddMsg="not revoked"
              modvalue=$value
          fi
          html_msg $ret $modvalue "${testname} (cert ${testUser} - $testAddMsg)" \
              "produced a returncode of $ret, expected is $modvalue"
      done
  done < ${SSLAUTH_TMP}
  rm -f ${SSLAUTH_TMP} ${SSL_REQ_FILE}
  
  html "</TABLE><BR>"
}


########################################################################
# local shell function to perform SSL Cipher Coverage tests of nss server
# by invoking remote test client on web server side.
# Invoked only if reverse testing is supported by web server.
# Params:
#      $1 - remote web server host
#      $2 - open port to connect to invoke CGI script
#      $3 - host where selfserv is running(name of the host nss tests
#           are running)
#      $4 - port where selfserv is running
#      $5 - selfserv nss db location
# No return value
#  
ssl_iopr_cov_ext_client()
{
  host=$1
  port=$2
  sslHost=$3
  sslPort=$4
  serDbDir=$5

  html_head "SSL Cipher Coverage of SelfServ $IOPR_HOSTADDR. $BYPASS_STRING $NORM_EXT"

  setValidCert
  ret=$?
  if [ $res -ne 0 ]; then
      html_failed "<TR><TD>Fail to find valid test cert(ws: $host)" 
      return $ret
  fi

  # P_R_SERVERDIR switch require for selfserv to work.
  # Will be restored after test
  OR_P_R_SERVERDIR=$P_R_SERVERDIR
  P_R_SERVERDIR=$serDbDir
  OR_P_R_CLIENTDIR=$P_R_CLIENTDIR
  P_R_CLIENTDIR=$serDbDir
  testname=""
  sparam="-vvvc ABCDEFcdefgijklmnvyz"
  # Launch the server
  start_selfserv 
  
  while read ecc tls param cipher therest; do
      [ -z "$ecc" -o "$ecc" = "#" -o "$ecc" = "ECC" ] && continue;
      echo "============= Beginning of the test ===================="
      echo
      
      is_selfserv_alive
      
      TEST_IN=${TMP}/${HOST}_IN.tmp.$$
      TEST_OUT=${TMP}/$HOST.tmp.$$
      rm -f $TEST_IN $TEST_OUT 2>/dev/null
      
      echo "GET $reverseRunCGIScript?host=$sslHost&port=$sslPort&cert=$testUser&cipher=$cipher HTTP/1.0" > $TEST_IN
      echo >> $TEST_IN
      
      echo "------- Request ----------------------"
      cat $TEST_IN
      echo "------- Command ----------------------"
      echo tstclnt -d $serDbDir -w ${R_PWFILE} -o -p $port \
          -h $host \< $TEST_IN \>\> $TEST_OUT

      tstclnt -d $serDbDir -w ${R_PWFILE} -o -p $port \
          -h $host <$TEST_IN > $TEST_OUT 

      echo "------- Server output Begin ----------"
      cat $TEST_OUT
      echo "------- Server output End   ----------"
      
      echo "Checking for errors in log file..."
      grep "SCRIPT=OK" $TEST_OUT 2>&1 >/dev/null
      if [ $? -eq 0 ]; then
          grep "cipher is not supported" $TEST_OUT 2>&1 >/dev/null
          if [ $? -eq 0 ]; then
              echo "Skiping test: no support for the cipher $cipher on server side"
              continue
          fi
          
          grep -i "SERVER ERROR:" $TEST_OUT
          ret=$?
          if [ $ret -eq 0 ]; then
              echo "Found problems. Reseting exit code to failure."
              
              ret=1
          else
              ret=0
          fi
      else
          echo "Script was not executed. Reseting exit code to failure."
          ret=11
      fi
      
      html_msg $ret 0 "Test ${cipher}. Server params: $sparam " \
          " produced a returncode of $ret, expected is 0"
      rm -f $TEST_OUT $TEST_IN 2>&1 > /dev/null
  done < ${SSLCOV}
  kill_selfserv
  
  P_R_SERVERDIR=$OR_P_R_SERVERDIR
  
  rm -f ${TEST_IN} ${TEST_OUT}
  html "</TABLE><BR>"
}

########################################################################
# local shell function to perform SSL Authentication tests of nss server
# by invoking remove test client on web server side
# Invoked only if reverse testing is supported by web server.
# Params:
#      $1 - remote web server host
#      $2 - open port to connect to invoke CGI script
#      $3 - host where selfserv is running(name of the host nss tests
#           are running)
#      $4 - port where selfserv is running
#      $5 - selfserv nss db location
# No return value
#  
ssl_iopr_auth_ext_client()
{
  host=$1
  port=$2
  sslHost=$3
  sslPort=$4
  serDbDir=$5

  html_head "SSL Client Authentication with Selfserv from $IOPR_HOSTADDR. $BYPASS_STRING $NORM_EXT"

  setValidCert
  ret=$?
  if [ $res -ne 0 ]; then
      html_failed "<TR><TD>Fail to find valid test cert(ws: $host)" 
      return $ret
  fi

  OR_P_R_SERVERDIR=$P_R_SERVERDIR
  P_R_SERVERDIR=${serDbDir}
  OR_P_R_CLIENTDIR=$P_R_CLIENTDIR
  P_R_CLIENTDIR=$serDbDir

  SSLAUTH_TMP=${TMP}/authin.tl.tmp

  grep -v "^#" $SSLAUTH | grep "\s*0\s*" > ${SSLAUTH_TMP}

  while read ecc value sparam cparam testname; do
      [ -z "$ecc" -o "$ecc" = "#" -o "$ecc" = "ECC" ] && continue;

      echo "Server params: $sparam"
      sparam=$sparam" -vvvc ABCDEFcdefgijklmnvyz"
      start_selfserv
      
      TEST_IN=${TMP}/$HOST_IN.tmp.$$
      TEST_OUT=${TMP}/$HOST.tmp.$$
      rm -f $TEST_IN $TEST_OUT 2>/dev/null

      echo "GET $reverseRunCGIScript?host=$sslHost&port=$sslPort&cert=$testUser HTTP/1.0" > $TEST_IN
      echo >> $TEST_IN
      
      echo "------- Request ----------------------"
      cat $TEST_IN
      echo "------- Command ----------------------"
      echo tstclnt -d $serDbDir -w ${R_PWFILE} -o -p $port \
          -h $host \< $TEST_IN \>\> $TEST_OUT
      
      tstclnt -d $serDbDir -w ${R_PWFILE} -o -p $port \
          -h $host <$TEST_IN > $TEST_OUT 
      
      echo "------- Server output Begin ----------"
      cat $TEST_OUT
      echo "------- Server output End   ----------"

      echo "Checking for errors in log file..."
      grep "SCRIPT=OK" $TEST_OUT 2>&1 >/dev/null
      if [ $? -eq 0 ]; then
          echo "Checking for error in log file..."
          grep -i "SERVER ERROR:" $TEST_OUT
          ret=$?
          if [ $ret -eq 0 ]; then
              echo "Found problems. Reseting exit code to failure."
              ret=1
          else
              ret=0
          fi
      else
          echo "Script was not executed. Reseting exit code to failure."
          ret=11
      fi
      
      html_msg $ret $value "${testname}. Server params: $sparam"\
          "produced a returncode of $ret, expected is $value"
      kill_selfserv
      rm -f $TEST_OUT $TEST_IN 2>&1 > /dev/null
  done < ${SSLAUTH_TMP}
  P_R_SERVERDIR=$OR_P_R_SERVERDIR

  rm -f ${SSLAUTH_TMP} ${TEST_IN} ${TEST_OUT}
  html "</TABLE><BR>"
}

#########################################################################
# local shell function to perform SSL CRL testing of nss server
# by invoking remote test client on web server side
# Invoked only if reverse testing is supported by web server.
# Params:
#      $1 - remote web server host
#      $2 - open port to connect to invoke CGI script
#      $3 - host where selfserv is running(name of the host nss tests
#           are running)
#      $4 - port where selfserv is running
#      $5 - selfserv nss db location
# No return value
#  
ssl_iopr_crl_ext_client()
{
  host=$1
  port=$2
  sslHost=$3
  sslPort=$4
  serDbDir=$5

  html_head "CRL SSL Selfserv Tests from $IOPR_HOSTADDR. $BYPASS_STRING $NORM_EXT"
  
  OR_P_R_SERVERDIR=$P_R_SERVERDIR
  P_R_SERVERDIR=${serDbDir}
  OR_P_R_CLIENTDIR=$P_R_CLIENTDIR
  P_R_CLIENTDIR=$serDbDir

  SSLAUTH_TMP=${TMP}/authin.tl.tmp
  grep -v "^#" $SSLAUTH | grep "\s*0\s*" > ${SSLAUTH_TMP}

  while read ecc value sparam _cparam testname; do
      [ -z "$ecc" -o "$ecc" = "#" -o "$ecc" = "ECC" ] && continue;
      sparam="$sparam  -vvvc ABCDEFcdefgijklmnvyz"
      start_selfserv

      for testUser in $SslClntValidCertName $SslClntRevokedCertName; do
	  
          is_selfserv_alive
          
          TEST_IN=${TMP}/${HOST}_IN.tmp.$$
          TEST_OUT=${TMP}/$HOST.tmp.$$
          rm -f $TEST_IN $TEST_OUT 2>/dev/null

          echo "GET $reverseRunCGIScript?host=$sslHost&port=$sslPort&cert=$testUser HTTP/1.0" > $TEST_IN
          echo >> $TEST_IN
          
          echo "------- Request ----------------------"
          cat $TEST_IN
          echo "------- Command ----------------------"
          echo tstclnt -d $serDbDir -w ${R_PWFILE} -o -p $port \
              -h ${host} \< $TEST_IN \>\> $TEST_OUT
            
          tstclnt -d $serDbDir -w ${R_PWFILE} -o -p $port \
              -h ${host} <$TEST_IN > $TEST_OUT 
          echo "------- Request ----------------------"
          cat $TEST_IN
          echo "------- Server output Begin ----------"
          cat $TEST_OUT
          echo "------- Server output End   ----------"
          
          echo "Checking for errors in log file..."
          grep "SCRIPT=OK" $TEST_OUT 2>&1 >/dev/null
          if [ $? -eq 0 ]; then
              grep -i "SERVER ERROR:" $TEST_OUT
              ret=$?
              if [ $ret -eq 0 ]; then
                  echo "Found problems. Reseting exit code to failure."
                  ret=1
              else
                  ret=0
              fi
          else
              echo "Script was not executed. Reseting exit code to failure."
              ret=11
          fi
          
          if [ "`echo $SslClntRevokedCertName | grep $testUser`" != "" ]; then
              modvalue=1
              testAddMsg="revoked"
          else
              testAddMsg="not revoked"
              modvalue=0
          fi
          
          html_msg $ret $modvalue "${testname} (cert ${testUser} - $testAddMsg)" \
		"produced a returncode of $ret, expected is $modvalue(selfserv args: $sparam)"
          rm -f $TEST_OUT $TEST_IN 2>&1 > /dev/null
      done
      kill_selfserv
  done < ${SSLAUTH_TMP}
  P_R_SERVERDIR=$OR_P_R_SERVERDIR

  rm -f ${SSLAUTH_TMP}
  html "</TABLE><BR>"
}

#####################################################################
# Initial point for running ssl test againt multiple hosts involved in
# interoperability testing. Called from nss/tests/ssl/ssl.sh
# It will only proceed with test run for a specific host if environment variable 
# IOPR_HOSTADDR_LIST was set, had the host name in the list
# and all needed file were successfully downloaded and installed for the host.
#
# Returns 1 if interoperability testing is off, 0 otherwise. 
#
ssl_iopr_run() {
    NO_ECC_CERTS=1 # disable ECC for interoperability tests

    if [ "$IOPR" -ne 1 ]; then
        return 1
    fi
    cd ${CLIENTDIR}

    num=1
    IOPR_HOST_PARAM=`echo "${IOPR_HOSTADDR_LIST} " | cut -f $num -d' '`
    while [ "$IOPR_HOST_PARAM" ]; do
        IOPR_HOSTADDR=`echo $IOPR_HOST_PARAM | cut -f 1 -d':'`
        IOPR_OPEN_PORT=`echo "$IOPR_HOST_PARAM:" | cut -f 2 -d':'`
        [ -z "$IOPR_OPEN_PORT" ] && IOPR_OPEN_PORT=443
        
        . ${IOPR_CADIR}_${IOPR_HOSTADDR}/iopr_server.cfg
        RES=$?
        
        if [ $RES -ne 0 -o X`echo "$wsFlags" | grep NOIOPR` != X ]; then
            num=`expr $num + 1`
            IOPR_HOST_PARAM=`echo "${IOPR_HOSTADDR_LIST} " | cut -f $num -d' '`
            continue
        fi
        
        #=======================================================
        # Check if server is capable to run ssl tests
        #
        [ -z "`echo ${supportedTests_new} | grep -i ssl`" ] && continue;

        # Testing directories defined by webserver.
        echo "Testing ssl interoperability.
                Client: local(tstclnt).
                Server: remote($IOPR_HOSTADDR:$IOPR_OPEN_PORT)"
        
        for sslTestType in ${supportedTests_new}; do
            if [ -z "`echo $sslTestType | grep -i ssl`" ]; then
                continue
            fi
            ssl_iopr_cov_ext_server $sslTestType ${IOPR_HOSTADDR} \
                ${IOPR_SSL_CLIENTDIR}_${IOPR_HOSTADDR}
            ssl_iopr_auth_ext_server $sslTestType ${IOPR_HOSTADDR} \
                ${IOPR_SSL_CLIENTDIR}_${IOPR_HOSTADDR}
            ssl_iopr_crl_ext_server $sslTestType ${IOPR_HOSTADDR} \
                ${IOPR_SSL_CLIENTDIR}_${IOPR_HOSTADDR}
        done
        
        
        # Testing selfserv with client located at the webserver.
        echo "Testing ssl interoperability.
                Client: remote($IOPR_HOSTADDR:$PORT)
                Server: local(selfserv)"
        ssl_iopr_cov_ext_client ${IOPR_HOSTADDR} ${IOPR_OPEN_PORT} \
            ${HOSTADDR} ${PORT} ${R_IOPR_SSL_SERVERDIR}_${IOPR_HOSTADDR}
        ssl_iopr_auth_ext_client ${IOPR_HOSTADDR} ${IOPR_OPEN_PORT} \
            ${HOSTADDR} ${PORT} ${R_IOPR_SSL_SERVERDIR}_${IOPR_HOSTADDR}
        ssl_iopr_crl_ext_client ${IOPR_HOSTADDR} ${IOPR_OPEN_PORT} \
            ${HOSTADDR} ${PORT} ${R_IOPR_SSL_SERVERDIR}_${IOPR_HOSTADDR}
        echo "================================================"
        echo "Done testing interoperability with $IOPR_HOSTADDR"
        num=`expr $num + 1`
        IOPR_HOST_PARAM=`echo "${IOPR_HOSTADDR_LIST} " | cut -f $num -d' '`
    done
    NO_ECC_CERTS=0
    return 0
}

