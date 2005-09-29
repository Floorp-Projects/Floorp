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
#
# mozilla/security/nss/tests/crmf/crmf.sh
#
# Script to test NSS crmf library (a static library) 
#
# needs to work on all Unix and Windows platforms
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
########################################################################

############################## smime_init ##############################
# local shell function to initialize this script
########################################################################
crmf_init()
{
  SCRIPTNAME=crmf.sh      # sourced - $0 would point to all.sh

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
  html_head "CRMF/CMMF Tests"

  # cmrf uses the S/MIME certs to test with
  grep "SUCCESS: SMIME passed" $CERT_LOG_FILE >/dev/null || {
      Exit 11 "Fatal - S/MIME of cert.sh needs to pass first"
  }

  CRMFDIR=${HOSTDIR}/crmf
  R_CRMFDIR=../crmf
  mkdir -p ${CRMFDIR}
  cd ${CRMFDIR}
}

############################## crmf_main ##############################
# local shell function to test basic CRMF request and CMMF responses
# from 1 --> 2"
########################################################################
crmf_main()
{
  echo "$SCRIPTNAME: CRMF/CMMF Tests ------------------------------"
  echo "crmftest -d ${P_R_BOBDIR} -p Bob -e dave@bogus.com -s TestCA -P nss crmf decode"
  crmftest -d ${P_R_BOBDIR} -p Bob -e dave@bogus.com -s TestCA -P nss crmf decode
  html_msg $? 0 "CRMF test" "."

  echo "crmftest -d ${P_R_BOBDIR} -p Bob -e dave@bogus.com -s TestCA -P nss cmmf"
  crmftest -d ${P_R_BOBDIR} -p Bob -e dave@bogus.com -s TestCA -P nss cmmf 
  html_msg $? 0 "CMMF test" "."

# Add tests for key recovery and challange as crmftest's capabilities increase

}
  
############################## crmf_cleanup ###########################
# local shell function to finish this script (no exit since it might be
# sourced)
########################################################################
crmf_cleanup()
{
  html "</TABLE><BR>"
  cd ${QADIR}
  . common/cleanup.sh
}

################## main #################################################

crmf_init
crmf_main
crmf_cleanup

