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

ocsp_stapling()
{
  # Parameter -4 is used as a temporary workaround for lack of IPv6 connectivity
  # on some build bot slaves.

  TESTNAME="startssl valid, supports OCSP stapling"
  echo "$SCRIPTNAME: $TESTNAME"
  echo "tstclnt -4 -V tls1.0: -T -v -F -M 1 -O -h kuix.de -p 5143 -d . < ${REQF}"
  ${BINDIR}/tstclnt -4 -V tls1.0: -T -v -F -M 1 -O -h kuix.de -p 5143 -d . < ${REQF}
  html_msg $? 0 "$TESTNAME"

  TESTNAME="startssl revoked, supports OCSP stapling"
  echo "$SCRIPTNAME: $TESTNAME"
  echo "tstclnt -4 -V tls1.0: -T -v -F -M 1 -O -h kuix.de -p 5144 -d . < ${REQF}"
  ${BINDIR}/tstclnt -4 -V tls1.0: -T -v -F -M 1 -O -h kuix.de -p 5144 -d . < ${REQF}
  html_msg $? 3 "$TESTNAME"

  TESTNAME="comodo trial test expired revoked, supports OCSP stapling"
  echo "$SCRIPTNAME: $TESTNAME"
  echo "tstclnt -4 -V tls1.0: -T -v -F -M 1 -O -h kuix.de -p 5145 -d . < ${REQF}"
  ${BINDIR}/tstclnt -4 -V tls1.0: -T -v -F -M 1 -O -h kuix.de -p 5145 -d . < ${REQF}
  html_msg $? 1 "$TESTNAME"

  TESTNAME="thawte (expired) valid, supports OCSP stapling"
  echo "$SCRIPTNAME: $TESTNAME"
  echo "tstclnt -4 -V tls1.0: -T -v -F -M 1 -O -h kuix.de -p 5146 -d . < ${REQF}"
  ${BINDIR}/tstclnt -4 -V tls1.0: -T -v -F -M 1 -O -h kuix.de -p 5146 -d . < ${REQF}
  html_msg $? 1 "$TESTNAME"

  TESTNAME="thawte (expired) revoked, supports OCSP stapling"
  echo "$SCRIPTNAME: $TESTNAME"
  echo "tstclnt -4 -V tls1.0: -T -v -F -M 1 -O -h kuix.de -p 5147 -d . < ${REQF}"
  ${BINDIR}/tstclnt -4 -V tls1.0: -T -v -F -M 1 -O -h kuix.de -p 5147 -d . < ${REQF}
  html_msg $? 1 "$TESTNAME"

  TESTNAME="digicert valid, supports OCSP stapling"
  echo "$SCRIPTNAME: $TESTNAME"
  echo "tstclnt -4 -V tls1.0: -T -v -F -M 1 -O -h kuix.de -p 5148 -d . < ${REQF}"
  ${BINDIR}/tstclnt -4 -V tls1.0: -T -v -F -M 1 -O -h kuix.de -p 5148 -d . < ${REQF}
  html_msg $? 0 "$TESTNAME"

  TESTNAME="digicert revoked, supports OCSP stapling"
  echo "$SCRIPTNAME: $TESTNAME"
  echo "tstclnt -4 -V tls1.0: -T -v -F -M 1 -O -h kuix.de -p 5149 -d . < ${REQF}"
  ${BINDIR}/tstclnt -4 -V tls1.0: -T -v -F -M 1 -O -h kuix.de -p 5149 -d . < ${REQF}
  html_msg $? 3 "$TESTNAME"

  TESTNAME="live valid, supports OCSP stapling"
  echo "$SCRIPTNAME: $TESTNAME"
  echo "tstclnt -V tls1.0: -T -v -F -M 1 -O -h login.live.com -p 443 -d . < ${REQF}"
  ${BINDIR}/tstclnt -V tls1.0: -T -v -F -M 1 -O -h login.live.com -p 443 -d . < ${REQF}
  html_msg $? 0 "$TESTNAME"

  TESTNAME="startssl valid, doesn't support OCSP stapling"
  echo "$SCRIPTNAME: $TESTNAME"
  echo "tstclnt -4 -V tls1.0: -T -v -F -M 1 -O -h kuix.de -p 443 -d . < ${REQF}"
  ${BINDIR}/tstclnt -4 -V tls1.0: -T -v -F -M 1 -O -h kuix.de -p 443 -d . < ${REQF}
  html_msg $? 2 "$TESTNAME"

  TESTNAME="cacert untrusted, doesn't support OCSP stapling"
  echo "$SCRIPTNAME: $TESTNAME"
  echo "tstclnt -V tls1.0: -T -v -F -M 1 -O -h www.cacert.org -p 443 -d . < ${REQF}"
  ${BINDIR}/tstclnt -V tls1.0: -T -v -F -M 1 -O -h www.cacert.org -p 443 -d . < ${REQF}
  html_msg $? 1 "$TESTNAME"
}

################## main #################################################
ocsp_init
ocsp_iopr_run
ocsp_stapling
