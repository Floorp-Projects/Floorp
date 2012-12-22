#! /bin/bash  
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
# mozilla/security/nss/tests/lowhash/lowhash.sh
#
# Script to test basic functionallity of the NSSLoHash API
#
# included from 
# --------------
#   all.sh
#
# needs to work on all Linux platforms
#
# tests implemented:
# lowash (verify encryption cert - bugzilla bug 119059)
#
# special strings
# ---------------
#
########################################################################

errors=0

############################## lowhash_init ##############################
# local shell function to initialize this script 
########################################################################
lowhash_init()
{
  SCRIPTNAME=lowhash.sh      # sourced - $0 would point to all.sh

  if [ -z "${CLEANUP}" ] ; then     # if nobody else is responsible for
      CLEANUP="${SCRIPTNAME}"       # cleaning this script will do it
  fi

  if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ]; then
      cd ../common
      . ./init.sh
  fi
  LOWHASHDIR=../lowhash
  mkdir -p ${LOWHASHDIR}
  if [ -f /proc/sys/crypto/fips_enabled ]; then
    FVAL=`cat /proc/sys/crypto/fips_enabled`
    html_head "Lowhash Tests - /proc/sys/crypto/fips_enabled is ${FVAL}"
  else
    html_head "Lowhash Tests"
  fi
  cd ${LOWHASHDIR}
}

############################## lowhash_test ##############################
# local shell function to test basic the NSS Low Hash API both in
# FIPS 140 compliant mode and not
########################################################################
lowhash_test()
{
  if [ ! -f ${BINDIR}/lowhashtest -a  \
       ! -f ${BINDIR}/lowhashtest${PROG_SUFFIX} ]; then
    echo "freebl lowhash not supported in this plaform."
  else
    TESTS="MD5 SHA1 SHA224 SHA256 SHA384 SHA512"
    OLD_MODE=`echo ${NSS_FIPS}`
    for fips_mode in 0 1; do
      echo "lowhashtest with fips mode=${fips_mode}"
      export NSS_FIPS=${fips_mode}
      for TEST in ${TESTS}
      do
        echo "lowhashtest ${TEST}"
        ${BINDIR}/lowhashtest ${TEST} 2>&1
        RESULT=$?
        html_msg ${RESULT} 0 "lowhashtest with fips mode=${fips_mode} for ${TEST}"
      done
    done
    export NSS_FIPS=${OLD_MODE}
  fi
}

############################## lowhash_cleanup ############################
# local shell function to finish this script (no exit since it might be 
# sourced)
########################################################################
lowhash_cleanup()
{
  html "</TABLE><BR>"
  cd ${QADIR}
  . common/cleanup.sh
}

################## main #################################################

lowhash_init
lowhash_test
lowhash_cleanup
echo "lowhash.sh done"
