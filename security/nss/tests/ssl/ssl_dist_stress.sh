#! /bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
#
# mozilla/security/nss/tests/ssl/ssl_dist_stress.sh
#
# Script to test NSS SSL - distributed stresstest - this script needs to 
# source the regular ssl.sh (for shellfunctions, certs and variables 
# initialisation)
# create certs
# start server
# start itself via rsh on different systems to connect back to the server
# 
#
# needs to work on all Unix and Windows platforms
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
########################################################################

############################## ssl_ds_init #############################
# local shell function to initialize this script
########################################################################
ssl_ds_init()
{
  if [ -z "$GLOB_MIN_CERT" ] ; then
      GLOB_MIN_CERT=0
  fi
  if [ -z "$GLOB_MAX_CERT" ] ; then
      GLOB_MAX_CERT=200
  fi
  IP_PARAM=""
  CD_QADIR_SSL=""


  if [ -n "$1" ] ; then
      ssl_ds_eval_opts $*
  fi
  SCRIPTNAME=ssl_dist_stress.sh      # sourced - $0 would point to all.sh

  if [ -z "${CLEANUP}" ] ; then     # if nobody else is responsible for
      CLEANUP="${SCRIPTNAME}"       # cleaning this script will do it
  fi
  
  ssl_init  # let some other script do the hard work (initialize, generate certs, ...

  SCRIPTNAME=ssl_dist_stress.sh
  echo "$SCRIPTNAME: SSL distributed stress tests ==============================="

}

######################### ssl_ds_usage #################################
# local shell function to explain the usage
########################################################################
ssl_ds_usage()
{
  echo "Usage: `basename $1`"
  echo "        -host hostname "
  echo "           ...host who runs the server, for distributed stress test"
  echo "        -stress "
  echo "           ...runs the server sider of the distributed stress test"
  echo "        -dir unixdirectory "
  echo "           ...lets the server side of the distributed stress test"
  echo "              know where to find the scritp to start on the remote side"
  echo "        -certnum start-end"
  echo "           ... provides the range of certs for distributed stress test"
  echo "               for example -certnum 10-20 will connect 10 times"
  echo "               no blanks in the range string (not 10 - 20)"
  echo "               valid range ${GLOB_MIN_CERT}-${GLOB_MAX_CERT}"
  echo "        -? ...prints this text"
  exit 1 #does not need to be Exit, very early in script
}

######################### ssl_ds_eval_opts #############################
# local shell function to deal with options and parameters
########################################################################
ssl_ds_eval_opts()
{
    #use $0 not $SCRIPTNAM<E, too early, SCRIPTNAME not yet set

  while [ -n "$1" ]
  do
    case $1 in
        -host)
            BUILD_OPT=1
            export BUILD_OPT
            DO_REM_ST="TRUE"
            shift
            SERVERHOST=$1
            HOST=$1
            if [ -z $SERVERHOST ] ; then
                echo "$0 `uname -n`: -host requires hostname"
                ssl_ds_usage
            fi
            echo "$0 `uname -n`: host $HOST ($1)"
            ;;
        -certn*)
            shift
            rangeOK=`echo $1  | sed -e 's/[0-9][0-9]*-[0-9][0-9]*/OK/'`
            MIN_CERT=`echo $1 | sed -e 's/-[0-9][0-9]*//' -e 's/^00*//'`
            MAX_CERT=`echo $1 | sed -e 's/[0-9][0-9]*-//' -e 's/^00*//'`
            if [ -z "$rangeOK" -o "$rangeOK" != "OK" -o \
                         -z "$MIN_CERT" -o -z "$MAX_CERT" -o \
                        "$MIN_CERT" -gt "$MAX_CERT" -o \
                        "$MIN_CERT" -lt "$GLOB_MIN_CERT" -o \
                        "$MAX_CERT" -gt "$GLOB_MAX_CERT" ] ; then
                echo "$0 `uname -n`: -certn range not valid"
                ssl_ds_usage
            fi
            echo "$0 `uname -n`: will use certs from $MIN_CERT to $MAX_CERT"
            ;;
        -server|-stress|-dist*st*)
            BUILD_OPT=1
            export BUILD_OPT
            DO_DIST_ST="TRUE"
            ;;
        -dir|-unixdir|-uxdir|-qadir)
            shift
            UX_DIR=$1
	    #FIXME - we need a default unixdir
            if [ -z "$UX_DIR" ] ; then # -o ! -d "$UX_DIR" ] ; then can't do, Win doesn't know...
                echo "$0 `uname -n`: -dir requires directoryname "
                ssl_ds_usage
            fi
            CD_QADIR_SSL="cd $UX_DIR"
            ;;
        -ip*)
            shift
            IP_ADDRESS=$1
            if [ -z "$IP_ADDRESS" ] ; then
                echo "$0 `uname -n`: -ip requires ip-address "
                ssl_ds_usage
            fi
            USE_IP=TRUE
            IP_PARAM="-ip $IP_ADDRESS"
            ;;
        -h|-help|"-?"|*)
            ssl_ds_usage
            ;;
    esac
    shift
  done
}

############################## ssl_ds_rem_stress #######################
# local shell function to perform the client part of the SSL stress test
########################################################################

ssl_ds_rem_stress()
{
  testname="SSL remote part of Stress test (`uname -n`)"
  echo "$SCRIPTNAME `uname -n`: $testname"

  #cp -r "${CLIENTDIR}" /tmp/ssl_ds.$$ #FIXME
  #cd /tmp/ssl_ds.$$
  #verbose="-v"

  cd ${CLIENTDIR}

  CONTINUE=$MAX_CERT
  while [ $CONTINUE -ge $MIN_CERT ]
  do
      echo "strsclnt -D -p ${PORT} -d ${P_R_CLIENTDIR} -w nss -c 1 $verbose  "
      echo "         -n TestUser$CONTINUE ${HOSTADDR} #`uname -n`"
      ${BINDIR}/strsclnt -D -p ${PORT} -d . -w nss -c 1 $verbose  \
               -n "TestUser$CONTINUE" ${HOSTADDR} &
               #${HOSTADDR} &
      CONTINUE=`expr $CONTINUE - 1 `
      #sleep 4 #give process time to start up
  done

  html_msg 0 0 "${testname}" #FIXME
}

######################### ssl_ds_dist_stress ###########################
# local shell function to perform the server part of the new, distributed 
# SSL stress test
########################################################################

ssl_ds_dist_stress()
{
  max_clientlist=" 
               box-200
               washer-200
               dryer-200
               hornet-50
               shabadoo-50
               y2sun2-10
               galileo-10
               shame-10
               axilla-10
               columbus-10
               smarch-10
               nugget-10
               charm-10
               hp64-10
               biggayal-10
               orville-10
               kwyjibo-10
               hbombaix-10
               raven-10
               jordan-10
               phaedrus-10
               louie-10
               trex-10
               compaqtor-10"

  #clientlist=" huey-2 dewey-2 hornet-2 shabadoo-2" #FIXME ADJUST
  clientlist="  box-200 washer-200 huey-200 dewey-200 hornet-200 shabadoo-200 louie-200"
  #clientlist="  box-2 huey-2 "
  #clientlist="washer-200 huey-200 dewey-200 hornet-200 "

  html_head "SSL Distributed Stress Test"

  testname="SSL distributed Stress test"

  echo cd "${CLIENTDIR}"
  cd "${CLIENTDIR}"
  if [ -z "CD_QADIR_SSL" ] ; then
      CD_QADIR_SSL="cd $QADIR/ssl"
  else
      cp -r $HOSTDIR $HOSTDIR/../../../../../booboo_Solaris8/mozilla/tests_results/security
  fi

  #sparam=" -t 128 -D -r "
  sparam=" -t 16 -D -r -r -y "
  start_selfserv

  for c in $clientlist
  do
      client=`echo $c | sed -e "s/-.*//"`
      number=`echo $c | sed -e "s/.*-//"`
      CLIENT_OK="TRUE"
      echo $client
      ping $client >/dev/null || CLIENT_OK="FALSE"
      if [ "$CLIENT_OK" = "FALSE" ] ; then
          echo "$SCRIPTNAME `uname -n`: $client can't be reached - skipping"
      else
          get_certrange $number
          echo "$SCRIPTNAME `uname -n`: $RSH $client -l svbld \\ "
          echo "       \" $CD_QADIR_SSL ;ssl_dist_stress.sh \\"
          echo "            -host $HOST -certnum $CERTRANGE $IP_PARAM \" "
          $RSH $client -l svbld \
               " $CD_QADIR_SSL;ssl_dist_stress.sh -host $HOST -certnum $CERTRANGE $IP_PARAM " &
      fi
  done

  echo cd "${CLIENTDIR}"
  cd "${CLIENTDIR}"

  sleep 500 # give the clients time to finish #FIXME ADJUST
 
  echo "GET /stop HTTP/1.0\n\n" > stdin.txt #check to make sure it has /r/n
  echo "tstclnt -h $HOSTADDR -p  8443 -d ${P_R_CLIENTDIR} -n TestUser0 "
  echo "        -w nss -f < stdin.txt"
  ${BINDIR}/tstclnt -h $HOSTADDR -p  8443 -d ${P_R_CLIENTDIR} -n TestUser0 \
	  -w nss -f < stdin.txt
  
  html_msg 0 0 "${testname}"
  html "</TABLE><BR>"
}

############################ get_certrange #############################
# local shell function to find the range of certs that the next remote 
# client is supposed to use (only for server side of the dist stress test
########################################################################
get_certrange()
{
  rangeOK=`echo $1  | sed -e 's/[0-9][0-9]*/OK/'`
  if [ -z "$rangeOK" -o "$rangeOK" != "OK" -o $1 = "OK" ] ; then
      range=10
      echo "$SCRIPTNAME `uname -n`: $1 is not a valid number of certs "
      echo "        defaulting to 10 for $client"
  else
      range=$1
      if [ $range -gt $GLOB_MAX_CERT ] ; then
          range=$GLOB_MAX_CERT
      fi
  fi
  if [ -z "$FROM_CERT" ] ; then    # start new on top of the cert stack
      FROM_CERT=$GLOB_MAX_CERT
  elif [ `expr $FROM_CERT - $range + 1 ` -lt  0 ] ; then 
          FROM_CERT=$GLOB_MAX_CERT # dont let it fall below 0 on the TO_CERT

  fi
  TO_CERT=`expr $FROM_CERT - $range + 1 `
  if [ $TO_CERT -lt 0 ] ; then     # it's not that I'm bad in math, I just 
      TO_CERT=0                    # don't trust expr...
  fi
  CERTRANGE="${TO_CERT}-${FROM_CERT}"
  FROM_CERT=`expr ${TO_CERT} - 1 ` #start the next  client one below 
}


################## main #################################################

DO_DIST_ST="TRUE"
. ./ssl.sh
ssl_ds_init $*
if [ -n  "$DO_REM_ST" -a "$DO_REM_ST" = "TRUE" ] ; then
    ssl_ds_rem_stress
    exit 0 #no cleanup on purpose
elif [ -n  "$DO_DIST_ST" -a "$DO_DIST_ST" = "TRUE" ] ; then
    ssl_ds_dist_stress
fi
ssl_cleanup
