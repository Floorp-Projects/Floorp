#! /bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
#
# mozilla/security/nss/tests/ocsp/ocsp.sh
#
# Script to test NSS OCSP
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
ocsp_init()
{
  SCRIPTNAME=ocsp.sh      # sourced - $0 would point to all.sh

  if [ -z "${CLEANUP}" ] ; then     # if nobody else is responsible for
      CLEANUP="${SCRIPTNAME}"       # cleaning this script will do it
  fi
  
  if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ]; then
      cd ../common
      . ./init.sh
  fi
  if [ -z "${IOPR_OCSP_SOURCED}" ]; then
      . ../iopr/ocsp_iopr.sh
  fi
  if [ ! -r $CERT_LOG_FILE ]; then  # we need certificates here
      cd ../cert
      . ./cert.sh
  fi
  SCRIPTNAME=ocsp.sh
  echo "$SCRIPTNAME: OCSP tests ==============================="

  REQF=${QADIR}/ssl/sslreq.dat

  cd ${CLIENTDIR}
}

################## main #################################################
ocsp_init
ocsp_iopr_run
