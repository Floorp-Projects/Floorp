#!/bin/sh

##############################################################
# do a cvs pull of NSS
##############################################################
cvs_pull()
{
  HOME=/share/builds/sbstools/nsstools/tmp; export HOME
  cd $HOME/..

  #do a CVS checkout of NSS, set your own CVSROOT
  cvs co mozilla/security/nss mozilla/security/coreconf
}

##############################################################
# build NSS after setting make variable NISCC_TEST
##############################################################
build_NSS()
{
  HOME=/share/builds/sbstools/nsstools/tmp; export HOME
  cd $HOME/..

  NISCC_TEST=1; export NISCC_TEST

  cd mozilla/security/coreconf
  gmake import >> $HOME/nisccBuildLog
  gmake >> $HOME/nisccBuildLog

  cd ../nss
  gmake import >> $HOME/nisccBuildLog
  gmake >> $HOME/nisccBuildLog
}


##############################################################
# set build dir, hostname, bin and lib directories
##############################################################
init()
{
  #
  #enable useful core files to be generated in case of crash
  #
  unlimit coredumpsize

  LOCALDIST=/share/builds/sbstools/nsstools/mozilla/dist; export LOCALDIST

  #
  # set appropriate hostname
  #
  DOMSUF=mcom.com
  if [ -z "$HOST" ]
  then
    HOST=`uname -n`
  fi
  HOSTNAME=$HOST.$DOMSUF; export HOSTNAME

  #
  # set bin and lib directory for Solaris and Linux
  #
  case `uname -s` in
  SunOS)
      LOCALDIST_BIN=${LOCALDIST}/SunOS5.8_DBG.OBJ/bin
      LOCALDIST_LIB=${LOCALDIST}/SunOS5.8_DBG.OBJ/lib
      echo $LOCALDIST_BIN
      ;;
  Linux)
      LOCALDIST_BIN=${LOCALDIST}/Linux2.4_x86_glibc_PTH_DBG.OBJ/bin
      LOCALDIST_LIB=${LOCALDIST}/Linux2.4_x86_glibc_PTH_DBG.OBJ/lib
      echo $LOCALDIST_BIN
      ;;
  HP-UX)
      LOCALDIST_BIN=${LOCALDIST}/HP-UXB.11.00_DBG.OBJ/bin
      LOCALDIST_LIB=${LOCALDIST}/HP-UXB.11.00_DBG.OBJ/lib
      echo $LOCALDIST_BIN
      ;;
  esac
  export LOCALDIST_BIN LOCALDIST_LIB

  PATH=/lib:/usr/lib:/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/tools/ns/bin:$LOCALDIST_LIB:$LOCALDIST_BIN; export PATH
  LD_LIBRARY_PATH=$LOCALDIST_LIB; export LD_LIBRARY_PATH

  echo "PATH" $PATH
  echo "LD_LIBRARY_PATH" $LD_LIBRARY_PATH

  #run strings command to verify NISCC_TEST was set and built
  echo "strings $LD_LIBRARY_PATH/libssl3.so | grep NISCC_TEST > $HOME/nisccLog 2>&1"
  strings $LD_LIBRARY_PATH/libssl3.so | grep NISCC_TEST > $HOME/nisccLog 2>&1

}
#end of init section


##############################################################
# Setup simple client and server directory
# NOTE: this function is not called in main, these dirs need 
# only be set up once
##############################################################
ssl_setup_dirs_simple()
{
  CLIENT=/share/builds/sbstools/nsstools/tmp/niscc_ssl/simple_client
  SERVER=/share/builds/sbstools/nsstools/tmp/niscc_ssl/simple_server

#
# Setup simple client directory
#
  rm -rf $CLIENT
  mkdir -p $CLIENT
  echo test > $CLIENT/password-is-test.txt
  certutil -N -d $CLIENT -f $CLIENT/password-is-test.txt
  certutil -A -d $CLIENT -n rootca -i $TEST/rootca.crt -t "C,C,"
  pk12util -i $TEST/client_crt.p12 -d $CLIENT -k $CLIENT/password-is-test.txt \
    -W testtest1
  certutil -L -d $CLIENT 

  echo "GET /stop HTTP/1.0" > $CLIENT/stop.txt
  echo ""                  >> $CLIENT/stop.txt

#
# Setup simple server directory
#
  rm -rf $SERVER
  mkdir -p $SERVER
  echo test > $SERVER/password-is-test.txt
  certutil -N -d $SERVER -f $SERVER/password-is-test.txt
  certutil -A -d $SERVER -n rootca -i $TEST/rootca.crt -t "TC,C,"
  pk12util -i $TEST/server_crt.p12 -d $SERVER -k $SERVER/password-is-test.txt \
    -W testtest1
  certutil -L -d $SERVER
}


##############################################################
# Setup resigned client and server directory
# NOTE: this function is not called in main, these dirs need 
# only be set up once
###############################################################
ssl_setup_dirs_resigned()
{
  CLIENT=/share/builds/sbstools/nsstools/tmp/niscc_ssl/resigned_client
  SERVER=/share/builds/sbstools/nsstools/tmp/niscc_ssl/resigned_server

#
# Setup resigned client directory
#
  rm -rf $CLIENT
  mkdir -p $CLIENT
  echo test > $CLIENT/password-is-test.txt
  certutil -N -d $CLIENT -f $CLIENT/password-is-test.txt
  certutil -A -d $CLIENT -n rootca -i $TEST/rootca.crt -t "C,C,"
  pk12util -i $TEST/client_crt.p12 -d $CLIENT -k $CLIENT/password-is-test.txt \
    -W testtest1
  certutil -L -d $CLIENT 

  echo "GET /stop HTTP/1.0" > $CLIENT/stop.txt
  echo ""                  >> $CLIENT/stop.txt

#
# Setup resigned server directory
#
  rm -rf $SERVER
  mkdir -p $SERVER
  echo test > $SERVER/password-is-test.txt
  certutil -N -d $SERVER -f $SERVER/password-is-test.txt
  certutil -A -d $SERVER -n rootca -i $TEST/rootca.crt -t "TC,C,"
  pk12util -i $TEST/server_crt.p12 -d $SERVER -k $SERVER/password-is-test.txt \
    -W testtest1
  certutil -L -d $SERVER
}


##############################################################
# NISCC SMIME tests
##############################################################
niscc_smime()
{
  cd /share/builds/sbstools/nsstools/tmp/NISCC_SMIME_testcases

  SMIME_CERT_DB_DIR=envDB; export SMIME_CERT_DB_DIR
  NSS_STRICT_SHUTDOWN=1; export NSS_STRICT_SHUTDOWN
  NSS_DISABLE_ARENA_FREE_LIST=1; export NSS_DISABLE_ARENA_FREE_LIST
  HOME=/share/builds/sbstools/nsstools/tmp/niscc_smime

  cmsutil -D -d $SMIME_CERT_DB_DIR -p testtest1 -b -i p7m-ed-m-files.txt > \
      $HOME/p7m-ed-m-results.txt 2>&1 &
 
  SMIME_CERT_DB_DIR=sigDB; export SMIME_CERT_DB_DIR

  cmsutil -D -d $SMIME_CERT_DB_DIR -c detached.txt -b -i p7m-sd-dt-files.txt > \
      $HOME/p7m-sd-dt-results.txt 2>&1 &

  cmsutil -D -d $SMIME_CERT_DB_DIR -b -i p7m-sd-op-files.txt > \
      $HOME/p7m-sd-op-results.txt 2>&1 &

}
#end of smime section

##############################################################
# set env variables for NISCC SSL tests
##############################################################
niscc_ssl_init()
{
  NSS_STRICT_SHUTDOWN=1; export NSS_STRICT_SHUTDOWN
  NSS_DISABLE_ARENA_FREE_LIST=1; export NSS_DISABLE_ARENA_FREE_LIST
  TEST=/share/builds/sbstools/nsstools/tmp/NISCC_SSL_testcases; export TEST
  HOME=/share/builds/sbstools/nsstools/tmp; export HOME
  cd $HOME
}


##############################################################
# do simple client auth tests
##############################################################
ssl_simple_client_auth()
{
  CLIENT=/share/builds/sbstools/nsstools/tmp/niscc_ssl/simple_client; export CLIENT
  SERVER=/share/builds/sbstools/nsstools/tmp/niscc_ssl/simple_server; export SERVER
  PORT=8443; export PORT
  START_AT=1; export START_AT
  STOP_AT=106160; export STOP_AT
  unset NISCC_TEST; export NISCC_TEST
  selfserv -p $PORT -d $SERVER -n server_crt -rr -t 5 -w test > $HOME/nisccLog1 2>&1 &

  NISCC_TEST=$TEST/simple_client; export NISCC_TEST
  strsclnt -d $CLIENT -n client_crt -p $PORT -t 4 -c 106160 -o -N -w test $HOSTNAME > $HOME/nisccLog2 2>&1

  unset NISCC_TEST; export NISCC_TEST
  echo "starting tstclnt to shutdown simple client selfserv process"
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog2 2>&1
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog2 2>&1
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog2 2>&1
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog2 2>&1
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog2 2>&1
  sleep 10

  find_core
}

##############################################################
# do simple server auth tests
##############################################################
ssl_simple_server_auth()
{
  CLIENT=/share/builds/sbstools/nsstools/tmp/niscc_ssl/simple_client; export CLIENT
  SERVER=/share/builds/sbstools/nsstools/tmp/niscc_ssl/simple_server; export SERVER
  PORT=8444; export PORT
  START_AT=1; export START_AT
  STOP_AT=106167; export STOP_AT
  NISCC_TEST=$TEST/simple_server; export NISCC_TEST
  selfserv -p $PORT -d $SERVER -n server_crt -t 5 -w test > $HOME/nisccLog3 2>&1 &

  unset NISCC_TEST; export NISCC_TEST
  strsclnt -d $CLIENT -p $PORT -t 4 -c 106167 -o -N $HOSTNAME > $HOME/nisccLog4 2>&1 

  echo "starting tstclnt to shutdown simple server selfserv process"
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog4 2>&1
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog4 2>&1
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog4 2>&1 
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog4 2>&1
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog4 2>&1
  sleep 10

  find_core
}

##############################################################
# do simple rootCA tests
##############################################################
ssl_simple_rootca()
{
  CLIENT=/share/builds/sbstools/nsstools/tmp/niscc_ssl/simple_client; export CLIENT
  SERVER=/share/builds/sbstools/nsstools/tmp/niscc_ssl/simple_server; export SERVER
  PORT=8445; export PORT
  START_AT=1; export START_AT
  STOP_AT=99960; export STOP_AT
  NISCC_TEST=$TEST/simple_rootca; export NISCC_TEST
  selfserv -p $PORT -d $SERVER -n server_crt -t 5 -w test > $HOME/nisccLog5 2>&1 &

  unset NISCC_TEST; export NISCC_TEST
  strsclnt -d $CLIENT -p $PORT -t 4 -c 99960 -o -N $HOSTNAME> $HOME/nisccLog6 2>&1

  echo "starting tstclnt to shutdown simple rootca selfserv process"
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog6 2>&1
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog6 2>&1
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog6 2>&1
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog6 2>&1
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog6 2>&1
  sleep 10

  find_core
}

##############################################################
# do resigned client auth tests
##############################################################
ssl_resigned_client_auth()
{
  CLIENT=/share/builds/sbstools/nsstools/tmp/niscc_ssl/resigned_client; export CLIENT
  SERVER=/share/builds/sbstools/nsstools/tmp/niscc_ssl/resigned_server; export SERVER
  PORT=8446; export PORT
  START_AT=0; export START_AT
  STOP_AT=99982; export STOP_AT
  unset NISCC_TEST; export NISCC_TEST
  selfserv -p $PORT -d $SERVER -n server_crt -rr -t 5 -w test > $HOME/nisccLog7 2>&1 &

  NISCC_TEST=$TEST/resigned_client; export NISCC_TEST
  strsclnt -d $CLIENT -n client_crt -p $PORT -t 4 -c 99982 -o -N -w test $HOSTNAME > $HOME/nisccLog8 2>&1 

  unset NISCC_TEST; export NISCC_TEST
  echo "starting tstclnt to shutdown resigned client selfserv process"
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog8 2>&1
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog8 2>&1
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog8 2>&1
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog8 2>&1
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog8 2>&1
  sleep 10

  find_core
}

##############################################################
# do resigned server auth tests
##############################################################
ssl_resigned_server_auth()
{
  CLIENT=/share/builds/sbstools/nsstools/tmp/niscc_ssl/resigned_client; export CLIENT
  SERVER=/share/builds/sbstools/nsstools/tmp/niscc_ssl/resigned_server; export SERVER
  PORT=8447; export PORT
  START_AT=0; export START_AT
  STOP_AT=99960; export STOP_AT
  NISCC_TEST=$TEST/resigned_server; export NISCC_TEST
  selfserv -p $PORT -d $SERVER -n server_crt -t 5 -w test > $HOME/nisccLog9 2>&1 &

  unset NISCC_TEST; export NISCC_TEST
  strsclnt -d $CLIENT -p $PORT -t 4 -c 99960 -o -N $HOSTNAME> $HOME/nisccLog10 2>&1 

  echo "starting tstclnt to shutdown resigned server selfserv process"
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog10 2>&1
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog10 2>&1
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog10 2>&1
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog10 2>&1
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog10 2>&1
  sleep 10

  find_core
}

##############################################################
# do resigned rootCA tests
##############################################################
ssl_resigned_rootca()
{
  CLIENT=/share/builds/sbstools/nsstools/tmp/niscc_ssl/resigned_client; export CLIENT
  SERVER=/share/builds/sbstools/nsstools/tmp/niscc_ssl/resigned_server; export SERVER
  PORT=8448; export PORT
  START_AT=0; export START_AT
  STOP_AT=100069; export STOP_AT
  NISCC_TEST=$TEST/resigned_rootca; export NISCC_TEST
  selfserv -p $PORT -d $SERVER -n server_crt -t 5 -w test > $HOME/nisccLog11 2>&1 &

  unset NISCC_TEST; export NISCC_TEST
  strsclnt -d $CLIENT -p $PORT -t 4 -c 100069 -o -N $HOSTNAME> $HOME/nisccLog12 2>&1 

  echo "starting tstclnt to shutdown resigned rootca selfserv process"
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog12 2>&1
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog12 2>&1
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog12 2>&1
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog12 2>&1
  tstclnt -h $HOSTNAME -p $PORT -d $CLIENT -n client_crt -o -f -w test < $CLIENT/stop.txt >> nisccLog12 2>&1
  sleep 10

  find_core
}

###############################################################
# find core file and email if found, move core file to save it
###############################################################
find_core()
{
  HOME=/share/builds/sbstools/nsstools/tmp; export HOME
  for w in `find $HOME -name "core" -print`
  do
      mv $w $w.`date +%H%M%S`
  done
}


###############################################################
# email the test logfile, and if core found, notify of failure
###############################################################
mail_testLog()
{
  HOME=/share/builds/sbstools/nsstools/tmp; export HOME
  find $HOME -name "core*" -print >> $HOME/crashLog
  SIZE=`cat $HOME/crashLog | wc -l`
  if [ "$SIZE" -gt 0 ]
  then
      cat $HOME/crashLog >> $HOME/nisccLogSummary
      /bin/mail -s "WEEKLY NISCC TESTS FAILED: check end of logfile" \
        nss-qa-report@mcom.com < $HOME/nisccLogSummary
  else
      /bin/mail -s "PASSED: weekly NISCC tests completed" \
        nss-qa-report@mcom.com < $HOME/nisccLogSummary
  fi
}

###############################################################
# append all logs to primary log
# NOTE: this results in a huge log,fills up available space
# and is decommissioned
###############################################################
append_all_logs()
{
  HOME=/share/builds/sbstools/nsstools/tmp; export HOME

  cat $HOME/NISCC_SMIME_testcases/p7m-ed-m-results.txt >> $HOME/nisccLog
  cat $HOME/NISCC_SMIME_testcases/p7m-sd-dt-results.txt >> $HOME/nisccLog
  cat $HOME/NISCC_SMIME_testcases/p7m-sd-op-results.txt >> $HOME/nisccLog

  cat $HOME/nisccLog1 >> $HOME/nisccLog
  cat $HOME/nisccLog2 >> $HOME/nisccLog
  cat $HOME/nisccLog3 >> $HOME/nisccLog
  cat $HOME/nisccLog4 >> $HOME/nisccLog
  cat $HOME/nisccLog5 >> $HOME/nisccLog
  cat $HOME/nisccLog6 >> $HOME/nisccLog
  cat $HOME/nisccLog7 >> $HOME/nisccLog
  cat $HOME/nisccLog8 >> $HOME/nisccLog
  cat $HOME/nisccLog9 >> $HOME/nisccLog
  cat $HOME/nisccLog10 >> $HOME/nisccLog
  cat $HOME/nisccLog11 >> $HOME/nisccLog
  cat $HOME/nisccLog12 >> $HOME/nisccLog
}

###############################################################
# summarise all logs
###############################################################
log_summary()
{
  HOME=/share/builds/sbstools/nsstools/tmp; export HOME

  for a in $HOME/nisccLog[0-9]*
      do echo ====================== $a
      grep -v using $a | sort | uniq -c | sort -b -n +0 -1
      done | tee $HOME/nisccLogSummary
}


###############################################################
# move the old mozilla and log files to save them, delete extra
# log files
###############################################################
move_files()
{
  HOME=/share/builds/sbstools/nsstools/tmp; export HOME

  cd $HOME
  mv -f ../mozilla ../mozilla.old
  mv nisccLogSummary nisccLogSummary.old
  mv crashLog crashLog.old
  mv nisccBuildLog nisccBuildLog.old

  rm -f nisccLog1 nisccLog2 nisccLog3 nisccLog4 nisccLog5 nisccLog6 \
      nisccLog7 nisccLog8 nisccLog9 nisccLog10 nisccLog11 nisccLog12
  rm -f NISCC_SMIME_testcases/p7m-ed-m-results.txt \
      NISCC_SMIME_testcases/p7m-sd-dt-results.txt \
      NISCC_SMIME_testcases/p7m-sd-op-results.txt
}

############################## main ##############################
cvs_pull
build_NSS
init
niscc_smime; find_core
niscc_ssl_init
  ssl_simple_client_auth
  ssl_simple_server_auth
  ssl_simple_rootca
  ssl_resigned_client_auth
  ssl_resigned_server_auth
  ssl_resigned_rootca
log_summary
mail_testLog
move_files
