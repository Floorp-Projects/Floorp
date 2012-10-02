#! /bin/sh  
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
# Script to run small tests to test specific crashes of NSS
#
# needs to work on all Unix and Windows platforms
#
# included from 
# --------------
#   all.sh
#
# tests implemented:
# vercrt (verify encryption cert - bugzilla bug 119059)
# vercrtfps (verify encryption cert in fips mode - bugzilla bug 119214)
# test3 (CERT_FindUserCertByUsage called 2nd time - bug 118864)
#
# special strings
# ---------------
#
########################################################################

############################## cmdtests_init ###########################
# local shell function to initialize this script 
########################################################################
cmdtests_init()
{
  SCRIPTNAME=cmdtests.sh      # sourced - $0 would point to all.sh

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
  SCRIPTNAME=cmdtests.sh
  html_head "Tests in cmd/tests"

# grep "SUCCESS: cmd/tests passed" $CERT_LOG_FILE >/dev/null || {
#     Exit 15 "Fatal - cert.sh needs to pass first"
# }

  CMDTESTSDIR=${HOSTDIR}/cmd/tests
  COPYDIR=${CMDTESTSDIR}/copydir

  R_CMDTESTSDIR=../cmd/tests
  R_COPYDIR=../cmd/tests/copydir
  P_R_COPYDIR=${R_COPYDIR}

  if [ -n "${MULTIACCESS_DBM}" ]; then
     P_R_COPYDIR="multiaccess:Cmdtests.$version"
  fi

  mkdir -p ${CMDTESTSDIR}
  mkdir -p ${COPYDIR}
  mkdir -p ${CMDTESTSDIR}/html

  cd ${CMDTESTSDIR}
}

############################## ct_vercrt ##################################
# CERT_VerifyCert should not fail when verifying encryption cert 
# Bugzilla Bug 119059
########################################################################
#ct_vercrt()
#{
 # echo "$SCRIPTNAME: Verify encryption certificate ----------------------"
 # echo "vercrt"
 # vercrt
 # ret=$?
 # html_msg $ret 0 "Verify encryption certificate (vercrt)"
#
#}


############################## cmdtests_cleanup ########################
# local shell function to finish this script (no exit since it might be 
# sourced)
########################################################################
cmdtests_cleanup()
{
  html "</TABLE><BR>"
  cd ${QADIR}
  . common/cleanup.sh
}

################## main #################################################

cmdtests_init

#ct_vercrt
cmdtests_cleanup
