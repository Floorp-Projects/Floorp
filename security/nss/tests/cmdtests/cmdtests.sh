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
