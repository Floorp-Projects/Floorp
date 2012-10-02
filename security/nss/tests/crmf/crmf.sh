#! /bin/bash  
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
  ${BINDIR}/crmftest -d ${P_R_BOBDIR} -p Bob -e dave@bogus.com -s TestCA -P nss crmf decode
  html_msg $? 0 "CRMF test" "."

  echo "crmftest -d ${P_R_BOBDIR} -p Bob -e dave@bogus.com -s TestCA -P nss cmmf"
  ${BINDIR}/crmftest -d ${P_R_BOBDIR} -p Bob -e dave@bogus.com -s TestCA -P nss cmmf 
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

