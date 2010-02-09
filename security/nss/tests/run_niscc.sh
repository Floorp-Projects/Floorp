#!/bin/bash
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
# Sun Microsystems, Inc..
# Portions created by the Initial Developer are Copyright (C) 2004-2009
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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

# This script runs NSS against test data provided by the United Kingdom
# National Infrastructure Security Co-ordination Centre (NISCC).
# The test data is not public and is not provided with this script.
########################################################################
# Base location to work from
NISCC_HOME=${NISCC_HOME:-/niscc}
export NISCC_HOME
# Set to where special NSS version exists. 
NSS_HACK=${NSS_HACK:-${NISCC_HOME}/nss_hack}; export NSS_HACK
# NSS version to test
LOCALDIST=${LOCALDIST:-/share/builds/sbstools/nsstools}
export LOCALDIST
# If true, do not rebuild client and server directories
NO_SETUP=${NO_SETUP:-"false"}; export NO_SETUP
# location of NISCC SSL testcases
TEST=${TEST:-"${NISCC_HOME}/NISCC_SSL_testcases"}; export TEST
# build the fully qualified domain name
# NOTE: domainname(1) may return the wrong domain in NIS environments
DOMSUF=${DOMSUF:-`domainname`}
if [ -z "$HOST" ]; then
	HOST=`uname -n`
fi
HOSTNAME=$HOST.$DOMSUF; export HOSTNAME
# Who and how to mail the output to
QA_LIST=${QA_LIST:-"nobody@localhost"}; export QA_LIST
MAIL_COMMAND=${MAIL_COMMAND:-/bin/mail}; export MAIL_COMMAND
# Whether or not to archive the logs
LOG_STORE=${LOG_STORE:-"false"}; export LOG_STORE


##############################################################
# set build dir, bin and lib directories
##############################################################
init()
{
  #enable useful core files to be generated in case of crash
  ulimit -c unlimited

# gmake is needed in the path for this suite to run
  PATH=/usr/bin:/usr/sbin:/usr/local/bin:/tools/ns/bin; export PATH
  echo "PATH $PATH" >> $NISCC_HOME/nisccLog00

  DISTTYPE=`cd ${NSS_HACK}/mozilla/security/nss/tests/common; gmake objdir_name`
  echo "DISTTYPE ${DISTTYPE}" >> $NISCC_HOME/nisccLog00
  HACKBIN=${NSS_HACK}/mozilla/dist/${DISTTYPE}/bin; export HACKBIN
  HACKLIB=${NSS_HACK}/mozilla/dist/${DISTTYPE}/lib; export HACKLIB

  TESTBIN=${LOCALDIST}/mozilla/dist/${DISTTYPE}/bin; export TESTBIN
  TESTLIB=${LOCALDIST}/mozilla/dist/${DISTTYPE}/lib; export TESTLIB

# Verify NISCC_TEST was set in the proper library
	if strings ${HACKLIB}/libssl3.so | grep NISCC_TEST > /dev/null 2>&1; then
		echo "${HACKLIB}/libssl3.so contains NISCC_TEST" >> \
$NISCC_HOME/nisccLog00
	else
		echo "${HACKLIB}/libssl3.so does NOT contain NISCC_TEST" >> \
$NISCC_HOME/nisccLog00
	fi

	if strings ${TESTLIB}/libssl3.so | grep NISCC_TEST > /dev/null 2>&1; then
		echo "${TESTLIB}/libssl3.so contains NISCC_TEST" >> \
$NISCC_HOME/nisccLog00
	else
		echo "${TESTLIB}/libssl3.so does NOT contain NISCC_TEST" >> \
$NISCC_HOME/nisccLog00
	fi
}
#end of init section


##############################################################
# Setup simple client and server directory
##############################################################
ssl_setup_dirs_simple()
{
  [ "$NO_SETUP" = "true" ] && return

  CLIENT=${NISCC_HOME}/niscc_ssl/simple_client
  SERVER=${NISCC_HOME}/niscc_ssl/simple_server

#
# Setup simple client directory
#
  rm -rf $CLIENT
  mkdir -p $CLIENT
  echo test > $CLIENT/password-is-test.txt
  LD_LIBRARY_PATH=${TESTLIB}; export LD_LIBRARY_PATH
  ${TESTBIN}/certutil -N -d $CLIENT -f $CLIENT/password-is-test.txt >> \
$NISCC_HOME/nisccLog00 2>&1
  ${TESTBIN}/certutil -A -d $CLIENT -n rootca -i $TEST/rootca.crt -t \
"C,C," >> $NISCC_HOME/nisccLog00 2>&1
  ${TESTBIN}/pk12util -i $TEST/client_crt.p12 -d $CLIENT -k \
$CLIENT/password-is-test.txt -W testtest1 >> $NISCC_HOME/nisccLog00 2>&1
  ${TESTBIN}/certutil -L -d $CLIENT >> $NISCC_HOME/nisccLog00 2>&1

  echo "GET /stop HTTP/1.0" > $CLIENT/stop.txt
  echo ""                  >> $CLIENT/stop.txt

#
# Setup simple server directory
#
  rm -rf $SERVER
  mkdir -p $SERVER
  echo test > $SERVER/password-is-test.txt
  ${TESTBIN}/certutil -N -d $SERVER -f $SERVER/password-is-test.txt \
>> $NISCC_HOME/nisccLog00 2>&1
  ${TESTBIN}/certutil -A -d $SERVER -n rootca -i $TEST/rootca.crt -t \
"TC,C," >> $NISCC_HOME/nisccLog00 2>&1
  ${TESTBIN}/pk12util -i $TEST/server_crt.p12 -d $SERVER -k \
$SERVER/password-is-test.txt -W testtest1 >> $NISCC_HOME/nisccLog00 2>&1
  ${TESTBIN}/certutil -L -d $SERVER >> $NISCC_HOME/nisccLog00 2>&1

  unset LD_LIBRARY_PATH
}


##############################################################
# Setup resigned client and server directory
###############################################################
ssl_setup_dirs_resigned()
{
  [ "$NO_SETUP" = "true" ] && return

  CLIENT=${NISCC_HOME}/niscc_ssl/resigned_client
  SERVER=${NISCC_HOME}/niscc_ssl/resigned_server

#
# Setup resigned client directory
#
  rm -rf $CLIENT
  mkdir -p $CLIENT
  echo test > $CLIENT/password-is-test.txt
  LD_LIBRARY_PATH=${TESTLIB}; export LD_LIBRARY_PATH
  ${TESTBIN}/certutil -N -d $CLIENT -f $CLIENT/password-is-test.txt \
>> $NISCC_HOME/nisccLog00 2>&1
  ${TESTBIN}/certutil -A -d $CLIENT -n rootca -i $TEST/rootca.crt -t \
"C,C," >> $NISCC_HOME/nisccLog00 2>&1
  ${TESTBIN}/pk12util -i $TEST/client_crt.p12 -d $CLIENT -k \
$CLIENT/password-is-test.txt -W testtest1 >> $NISCC_HOME/nisccLog00 2>&1
  ${TESTBIN}/certutil -L -d $CLIENT  >> $NISCC_HOME/nisccLog00 2>&1

  echo "GET /stop HTTP/1.0" > $CLIENT/stop.txt
  echo ""                  >> $CLIENT/stop.txt

#
# Setup resigned server directory
#
  rm -rf $SERVER
  mkdir -p $SERVER
  echo test > $SERVER/password-is-test.txt
  ${TESTBIN}/certutil -N -d $SERVER -f $SERVER/password-is-test.txt \
>> $NISCC_HOME/nisccLog00 2>&1
  ${TESTBIN}/certutil -A -d $SERVER -n rootca -i $TEST/rootca.crt -t \
"TC,C," >> $NISCC_HOME/nisccLog00 2>&1
  ${TESTBIN}/pk12util -i $TEST/server_crt.p12 -d $SERVER -k \
$SERVER/password-is-test.txt -W testtest1 >> $NISCC_HOME/nisccLog00 2>&1
  ${TESTBIN}/certutil -L -d $SERVER >> $NISCC_HOME/nisccLog00 2>&1

  unset LD_LIBRARY_PATH
}


##############################################################
# NISCC SMIME tests
##############################################################
niscc_smime()
{
  cd ${NISCC_HOME}/NISCC_SMIME_testcases

  if [ ! -d ${NISCC_HOME}/niscc_smime ]; then
	mkdir -p ${NISCC_HOME}/niscc_smime
  fi

  SMIME_CERT_DB_DIR=envDB; export SMIME_CERT_DB_DIR
  NSS_STRICT_SHUTDOWN=1; export NSS_STRICT_SHUTDOWN
  NSS_DISABLE_ARENA_FREE_LIST=1; export NSS_DISABLE_ARENA_FREE_LIST
  LD_LIBRARY_PATH=${TESTLIB}; export LD_LIBRARY_PATH

# Generate envDB if needed
  if [ ! -d $SMIME_CERT_DB_DIR ]; then
	mkdir -p $SMIME_CERT_DB_DIR
 	echo testtest1 > password-is-testtest1.txt
	${TESTBIN}/certutil -N -d ./${SMIME_CERT_DB_DIR} -f \
password-is-testtest1.txt > /dev/null 2>&1
	${TESTBIN}/certutil -A -d $SMIME_CERT_DB_DIR -i CA.crt -n CA -t "TC,C,"
	${TESTBIN}/certutil -A -d $SMIME_CERT_DB_DIR -i Client.crt -n Client \
-t "TC,C,"
	${TESTBIN}/pk12util -i ./CA.p12 -d $SMIME_CERT_DB_DIR -k \
./password-is-test.txt -W testtest1 
	${TESTBIN}/pk12util -i ./Client.p12 -d $SMIME_CERT_DB_DIR -k \
./password-is-test.txt -W testtest1 
  fi

# if p7m-ed-m-files.txt does not exist, then generate it.
  if [ ! -f p7m-ed-m-files.txt ]; then
	find ./p7m-ed-m-0* -type f -print >> p7m-ed-m-files.txt
  fi

  ${TESTBIN}/cmsutil -D -d $SMIME_CERT_DB_DIR -p testtest1 -b -i \
p7m-ed-m-files.txt > $NISCC_HOME/niscc_smime/p7m-ed-m-results.txt 2>&1 
 
  SMIME_CERT_DB_DIR=sigDB; export SMIME_CERT_DB_DIR
# Generate sigDB if needed
  if [ ! -d $SMIME_CERT_DB_DIR ]; then
	mkdir -p $SMIME_CERT_DB_DIR
	${TESTBIN}/certutil -N -d $SMIME_CERT_DB_DIR -f password-is-testtest1.txt
	${TESTBIN}/certutil -A -d $SMIME_CERT_DB_DIR -i CA.crt -n CA -t "TC,C,"
	${TESTBIN}/certutil -A -d $SMIME_CERT_DB_DIR -i Client.crt -n Client \
-t "TC,C,"
  fi

# if p7m-sd-dt-files.txt does not exist, then generate it.
  if [ ! -f p7m-sd-dt-files.txt ]; then
	find ./p7m-sd-dt-[cm]-* -type f -print >> p7m-sd-dt-files.txt
  fi

  if [ ! -f detached.txt ]; then
	touch detached.txt
  fi

  ${TESTBIN}/cmsutil -D -d $SMIME_CERT_DB_DIR -c detached.txt -b -i \
p7m-sd-dt-files.txt > $NISCC_HOME/niscc_smime/p7m-sd-dt-results.txt 2>&1 

# if p7m-sd-op-files.txt does not exist, then generate it.
  if [ ! -f p7m-sd-op-files.txt ]; then
	find ./p7m-sd-op-[cm]-* -type f -print >> p7m-sd-op-files.txt
  fi

  ${TESTBIN}/cmsutil -D -d $SMIME_CERT_DB_DIR -b -i p7m-sd-op-files.txt > \
$NISCC_HOME/niscc_smime/p7m-sd-op-results.txt 2>&1 

  unset LD_LIBRARY_PATH

  find_core
}
#end of smime section

##############################################################
# set env variables for NISCC SSL tests
##############################################################
niscc_ssl_init()
{
  NSS_STRICT_SHUTDOWN=1; export NSS_STRICT_SHUTDOWN
  NSS_DISABLE_ARENA_FREE_LIST=1; export NSS_DISABLE_ARENA_FREE_LIST
  cd $NISCC_HOME
}


##############################################################
# do simple client auth tests
# Use an altered client against the server
##############################################################
ssl_simple_client_auth()
{
  CLIENT=${NISCC_HOME}/niscc_ssl/simple_client; export CLIENT
  SERVER=${NISCC_HOME}/niscc_ssl/simple_server; export SERVER
  PORT=8443; export PORT
  START_AT=1; export START_AT
  STOP_AT=106160; export STOP_AT
  unset NISCC_TEST; export NISCC_TEST
  LD_LIBRARY_PATH=${TESTLIB}; export LD_LIBRARY_PATH
  ${TESTBIN}/selfserv -p $PORT -d $SERVER -n server_crt -rr -t 5 -w test > \
$NISCC_HOME/nisccLog01 2>&1 &
  sleep 10

  NISCC_TEST=$TEST/simple_client; export NISCC_TEST
  LD_LIBRARY_PATH=${HACKLIB}; export LD_LIBRARY_PATH
  ${HACKBIN}/strsclnt -d $CLIENT -n client_crt -p $PORT -t 4 -c 106160 -o -N \
-w test $HOSTNAME > $NISCC_HOME/nisccLog02 2>&1

  unset NISCC_TEST; export NISCC_TEST
  echo "starting tstclnt to shutdown simple client selfserv process"
  ${HACKBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w \
test < $CLIENT/stop.txt >> nisccLog02 2>&1
  ${HACKBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w \
test < $CLIENT/stop.txt >> nisccLog02 2>&1
  ${HACKBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w \
test < $CLIENT/stop.txt >> nisccLog02 2>&1
  ${HACKBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w \
test < $CLIENT/stop.txt >> nisccLog02 2>&1
  ${HACKBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w \
test < $CLIENT/stop.txt >> nisccLog02 2>&1

  unset LD_LIBRARY_PATH

  sleep 10

  find_core
}

##############################################################
# do simple server auth tests
# Use an altered server against the client
##############################################################
ssl_simple_server_auth()
{
  CLIENT=${NISCC_HOME}/niscc_ssl/simple_client; export CLIENT
  SERVER=${NISCC_HOME}/niscc_ssl/simple_server; export SERVER
  PORT=8444; export PORT
  START_AT=1; export START_AT
  STOP_AT=106167; export STOP_AT
  LD_LIBRARY_PATH=${HACKLIB}; export LD_LIBRARY_PATH
  NISCC_TEST=$TEST/simple_server; export NISCC_TEST
  ${HACKBIN}/selfserv -p $PORT -d $SERVER -n server_crt -t 5 -w test > \
$NISCC_HOME/nisccLog03 2>&1 &

  unset NISCC_TEST; export NISCC_TEST
  LD_LIBRARY_PATH=${TESTLIB}; export LD_LIBRARY_PATH
  ${TESTBIN}/strsclnt -d $CLIENT -p $PORT -t 4 -c 106167 -o -N $HOSTNAME > \
$NISCC_HOME/nisccLog04 2>&1 

  echo "starting tstclnt to shutdown simple server selfserv process"
  ${TESTBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f \
-w test < $CLIENT/stop.txt >> nisccLog04 2>&1
  ${TESTBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f \
-w test < $CLIENT/stop.txt >> nisccLog04 2>&1
  ${TESTBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f \
-w test < $CLIENT/stop.txt >> nisccLog04 2>&1 
  ${TESTBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f \
-w test < $CLIENT/stop.txt >> nisccLog04 2>&1
  ${TESTBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f \
-w test < $CLIENT/stop.txt >> nisccLog04 2>&1

  unset LD_LIBRARY_PATH

  sleep 10

  find_core
}

##############################################################
# do simple rootCA tests
# Use an altered server against the client
##############################################################
ssl_simple_rootca()
{
  CLIENT=${NISCC_HOME}/niscc_ssl/simple_client; export CLIENT
  SERVER=${NISCC_HOME}/niscc_ssl/simple_server; export SERVER
  PORT=8445; export PORT
  START_AT=1; export START_AT
  STOP_AT=106190; export STOP_AT
  LD_LIBRARY_PATH=${HACKLIB}; export LD_LIBRARY_PATH
  NISCC_TEST=$TEST/simple_rootca; export NISCC_TEST
  ${HACKBIN}/selfserv -p $PORT -d $SERVER -n server_crt -t 5 -w test > \
$NISCC_HOME/nisccLog05 2>&1 &

  unset NISCC_TEST; export NISCC_TEST
  LD_LIBRARY_PATH=${TESTLIB}; export LD_LIBRARY_PATH
  ${TESTBIN}/strsclnt -d $CLIENT -p $PORT -t 4 -c 106190 -o -N $HOSTNAME > \
$NISCC_HOME/nisccLog06 2>&1

  echo "starting tstclnt to shutdown simple rootca selfserv process"
  ${TESTBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f \
-w test < $CLIENT/stop.txt >> nisccLog06 2>&1
  ${TESTBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f \
-w test < $CLIENT/stop.txt >> nisccLog06 2>&1
  ${TESTBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f \
-w test < $CLIENT/stop.txt >> nisccLog06 2>&1
  ${TESTBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f \
-w test < $CLIENT/stop.txt >> nisccLog06 2>&1
  ${TESTBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f \
-w test < $CLIENT/stop.txt >> nisccLog06 2>&1

  unset LD_LIBRARY_PATH
  sleep 10

  find_core
}

##############################################################
# do resigned client auth tests
# Use an altered client against the server
##############################################################
ssl_resigned_client_auth()
{
  CLIENT=${NISCC_HOME}/niscc_ssl/resigned_client; export CLIENT
  SERVER=${NISCC_HOME}/niscc_ssl/resigned_server; export SERVER
  PORT=8446; export PORT
  START_AT=0; export START_AT
  STOP_AT=99981; export STOP_AT
  unset NISCC_TEST; export NISCC_TEST
  LD_LIBRARY_PATH=${TESTLIB}; export LD_LIBRARY_PATH
  ${TESTBIN}/selfserv -p $PORT -d $SERVER -n server_crt -rr -t 5 -w test > \
$NISCC_HOME/nisccLog07 2>&1 &

  NISCC_TEST=$TEST/resigned_client; export NISCC_TEST
  LD_LIBRARY_PATH=${HACKLIB}; export LD_LIBRARY_PATH
  ${HACKBIN}/strsclnt -d $CLIENT -n client_crt -p $PORT -t 4 -c 99982 -o -N \
-w test $HOSTNAME > $NISCC_HOME/nisccLog08 2>&1 

  unset NISCC_TEST; export NISCC_TEST
  echo "starting tstclnt to shutdown resigned client selfserv process"
  ${HACKBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f \
-w test < $CLIENT/stop.txt >> nisccLog08 2>&1
  ${HACKBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f \
-w test < $CLIENT/stop.txt >> nisccLog08 2>&1
  ${HACKBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f \
-w test < $CLIENT/stop.txt >> nisccLog08 2>&1
  ${HACKBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f \
-w test < $CLIENT/stop.txt >> nisccLog08 2>&1
  ${HACKBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f \
-w test < $CLIENT/stop.txt >> nisccLog08 2>&1

  unset LD_LIBRARY_PATH

  sleep 10

  find_core
}

##############################################################
# do resigned server auth tests
# Use an altered server against the client
##############################################################
ssl_resigned_server_auth()
{
  CLIENT=${NISCC_HOME}/niscc_ssl/resigned_client; export CLIENT
  SERVER=${NISCC_HOME}/niscc_ssl/resigned_server; export SERVER
  PORT=8447; export PORT
  START_AT=0; export START_AT
  STOP_AT=100068; export STOP_AT
  LD_LIBRARY_PATH=${HACKLIB}; export LD_LIBRARY_PATH
  NISCC_TEST=$TEST/resigned_server; export NISCC_TEST
  ${HACKBIN}/selfserv -p $PORT -d $SERVER -n server_crt -t 5 -w test > \
$NISCC_HOME/nisccLog09 2>&1 &

  unset NISCC_TEST; export NISCC_TEST
  LD_LIBRARY_PATH=${TESTLIB}; export LD_LIBRARY_PATH
  ${TESTBIN}/strsclnt -d $CLIENT -p $PORT -t 4 -c 100069 -o -N $HOSTNAME > \
$NISCC_HOME/nisccLog10 2>&1 

  echo "starting tstclnt to shutdown resigned server selfserv process"
  ${TESTBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f \
-w test < $CLIENT/stop.txt >> nisccLog10 2>&1
  ${TESTBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f \
-w test < $CLIENT/stop.txt >> nisccLog10 2>&1
  ${TESTBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f \
-w test < $CLIENT/stop.txt >> nisccLog10 2>&1
  ${TESTBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f \
-w test < $CLIENT/stop.txt >> nisccLog10 2>&1
  ${TESTBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f \
-w test < $CLIENT/stop.txt >> nisccLog10 2>&1

  unset LD_LIBRARY_PATH

  sleep 10

  find_core
}

##############################################################
# do resigned rootCA tests
# Use an altered server against the client
##############################################################
ssl_resigned_rootca()
{
  CLIENT=${NISCC_HOME}/niscc_ssl/resigned_client; export CLIENT
  SERVER=${NISCC_HOME}/niscc_ssl/resigned_server; export SERVER
  PORT=8448; export PORT
  START_AT=0; export START_AT
  STOP_AT=99959; export STOP_AT
  LD_LIBRARY_PATH=${HACKLIB}; export LD_LIBRARY_PATH
  NISCC_TEST=$TEST/resigned_rootca; export NISCC_TEST
  ${HACKBIN}/selfserv -p $PORT -d $SERVER -n server_crt -t 5 -w test > \
$NISCC_HOME/nisccLog11 2>&1 &

  unset NISCC_TEST; export NISCC_TEST
  LD_LIBRARY_PATH=${TESTLIB}; export LD_LIBRARY_PATH
  ${TESTBIN}/strsclnt -d $CLIENT -p $PORT -t 4 -c 99960 -o -N $HOSTNAME > \
$NISCC_HOME/nisccLog12 2>&1 

  echo "starting tstclnt to shutdown resigned rootca selfserv process"
  ${TESTBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f \
-w test < $CLIENT/stop.txt >> nisccLog12 2>&1
  ${TESTBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f \
-w test < $CLIENT/stop.txt >> nisccLog12 2>&1
  ${TESTBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f \
-w test < $CLIENT/stop.txt >> nisccLog12 2>&1
  ${TESTBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f \
-w test < $CLIENT/stop.txt >> nisccLog12 2>&1
  ${TESTBIN}/tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f \
-w test < $CLIENT/stop.txt >> nisccLog12 2>&1

  unset LD_LIBRARY_PATH

  sleep 10

  find_core
}

###############################################################
# find core file and email if found, move core file to save it
###############################################################
find_core()
{
  for w in `find $NISCC_HOME -name "core" -print`
  do
      mv $w $w.`date +%H%M%S`
  done
}


###############################################################
# NISCC tests result in a failure only if a core file is produced
# Mail out the status - the log summary is not mailed due to its 
# size of about one hundred megabytes.
###############################################################
mail_testLog()
{
  # remove mozilla nss build false positives and core stored in previous runs
  find $NISCC_HOME -name "core*" -print | grep -v coreconf | grep -v \
core_watch | grep -v archive >> $NISCC_HOME/crashLog
  SIZE=`cat $NISCC_HOME/crashLog | wc -l`
  if [ "$SIZE" -gt 0 ]; then
      cat $NISCC_HOME/crashLog >> $NISCC_HOME/nisccLogSummary
      $MAIL_COMMAND -s "WEEKLY NISCC TESTS FAILED: check end of logfile" \
        $QA_LIST < /dev/null
  else
      $MAIL_COMMAND -s "PASSED: weekly NISCC tests completed" \
        $QA_LIST < /dev/null
  fi
}


###############################################################
# summarise all logs
###############################################################
log_summary()
{

  for a in $NISCC_HOME/nisccLog[0-9]*
      do echo ================================== $a
      grep -v using $a | sort | uniq -c | sort -b -n +0 -1
      done | tee $NISCC_HOME/nisccLogSummary

  for a in $NISCC_HOME/niscc_smime/p7m-*-results.txt
      do echo ================================== $a
      grep -v using $a | sort | uniq -c | sort -b -n +0 -1
      done | tee -a $NISCC_HOME/nisccLogSummary
}


###############################################################
# move the old mozilla and log files to save them, delete extra
# log files
###############################################################
move_files()
{

  cd $NISCC_HOME

  if [ "$LOG_STORE" = "true" ]; then
	# Check for archive directory
	if [ ! -d $NISCC_HOME/archive ]; then
		mkdir -p $NISCC_HOME/archive
	fi
	# Determine next log storage point
	DATE=`date +%Y%m%d`
	SLOT=`ls -1 $NISCC_HOME/archive | grep ${DATE} | wc -l`
	SLOT=`expr $SLOT + 1`
	LOCATION=$NISCC_HOME/archive/${DATE}.${SLOT}
	mkdir -p ${LOCATION}
	# Archive the logs
	mv nisccBuildLog ${LOCATION} 2> /dev/null
	mv nisccLogSummary ${LOCATION}
	mv nisccLog00 ${LOCATION}
	mv nisccLog01 ${LOCATION}
	mv nisccLog02 ${LOCATION}
	mv nisccLog03 ${LOCATION}
	mv nisccLog04 ${LOCATION}
	mv nisccLog05 ${LOCATION}
	mv nisccLog06 ${LOCATION}
	mv nisccLog07 ${LOCATION}
	mv nisccLog08 ${LOCATION}
	mv nisccLog09 ${LOCATION}
	mv nisccLog10 ${LOCATION}
	mv nisccLog11 ${LOCATION}
	mv nisccLog12 ${LOCATION}
	mv niscc_smime/p7m-ed-m-results.txt ${LOCATION}
	mv niscc_smime/p7m-sd-dt-results.txt ${LOCATION}
	mv niscc_smime/p7m-sd-op-results.txt ${LOCATION}
	# Archive any core files produced
	for CORE in `cat $NISCC_HOME/crashLog`
	do
		mv $CORE ${LOCATION}
	done
	mv crashLog ${LOCATION}
	return
  fi

  if [ -d ../mozilla ]; then
	mv -f ../mozilla ../mozilla.old
  	mv nisccBuildLog nisccBuildLog.old
  fi
  mv nisccLogSummary nisccLogSummary.old
  mv crashLog crashLog.old

  rm -f nisccLog00 nisccLog01 nisccLog02 nisccLog03 nisccLog04 nisccLog05 \
nisccLog06 nisccLog07 nisccLog08 nisccLog09 nisccLog10 nisccLog11 nisccLog12
  rm -f niscc_smime/p7m-ed-m-results.txt \
      niscc_smime/p7m-sd-dt-results.txt \
      niscc_smime/p7m-sd-op-results.txt
}

############################## main ##############################
init
niscc_smime
niscc_ssl_init
ssl_setup_dirs_simple
  ssl_simple_client_auth
  ssl_simple_server_auth
  ssl_simple_rootca
ssl_setup_dirs_resigned
  ssl_resigned_client_auth
  ssl_resigned_server_auth
  ssl_resigned_rootca
log_summary
mail_testLog
move_files
