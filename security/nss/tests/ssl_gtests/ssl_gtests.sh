#! /bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
#
# tests/ssl_gtests/ssl_gtests.sh
#
# Script to drive the ssl gtest unit tests
#
# needs to work on all Unix and Windows platforms
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
########################################################################
ssl_gtest_certs() {
  mkdir -p "${SSLGTESTDIR}"
  cd "${SSLGTESTDIR}"

  PROFILEDIR=`pwd`
  if [ "${OS_ARCH}" = "WINNT" -a "$OS_NAME" = "CYGWIN_NT" ]; then
    PROFILEDIR=`cygpath -m "${PROFILEDIR}"`
  fi

  ${BINDIR}/certutil -N -d "${PROFILEDIR}" --empty-password 2>&1
  html_msg $? 0 "create ssl_gtest database"

  ${BINDIR}/certutil -S -z ${R_NOISE_FILE} -d "${PROFILEDIR}" \
    -n server -s "CN=server" -t C,C,C -x -m 1 -w -2 -v 120 \
    -k rsa -g 1024 -Z SHA256 -1 -2 <<CERTSCRIPT
0
2
9
n
n

n
CERTSCRIPT
  html_msg $? 0 "create ssl_gtest server certificate"

  ${BINDIR}/certutil -S -z ${R_NOISE_FILE} -d "${PROFILEDIR}" \
    -n client -s "CN=client" -t C,C,C -x -m 1 -w -2 -v 120 \
    -k rsa -g 1024 -Z SHA256 -1 -2 <<CERTSCRIPT
0
9
n
n

n
CERTSCRIPT
  html_msg $? 0 "create ssl_gtest client certificate"

  ${BINDIR}/certutil -S -z ${R_NOISE_FILE} -d "${PROFILEDIR}" \
    -n ecdsa -s "CN=ecdsa" -t C,C,C -x -m 1 -w -2 -v 120 \
    -k ec -q nistp256 -Z SHA256 -1 -2 <<CERTSCRIPT
0
9
n
n

n
CERTSCRIPT
  html_msg $? 0 "create ssl_gtest ECDSA certificate"
}

############################## ssl_gtest_init ##########################
# local shell function to initialize this script
########################################################################
ssl_gtest_init()
{
  SCRIPTNAME=ssl_gtest.sh      # sourced - $0 would point to all.sh

  if [ -z "${CLEANUP}" ] ; then     # if nobody else is responsible for
      CLEANUP="${SCRIPTNAME}"       # cleaning this script will do it
  fi
  if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ]; then
      cd ../common
      . ./init.sh
  fi

  SCRIPTNAME=ssl_gtest.sh
  html_head SSL Gtests

  if [ ! -d "${SSLGTESTDIR}" ]; then
    ssl_gtest_certs
  fi

  REQF=${QADIR}/ssl/sslreq.dat

  cd "${SSLGTESTDIR}"
}

########################## ssl_gtest_start #########################
# Local function to actually start the test
####################################################################
ssl_gtest_start()
{
  if [ ! -f ${BINDIR}/ssl_gtest ]; then
    html_unknown "Skipping ssl_gtest (not built)"
    return
  fi

  # Temporarily disable asserts for PKCS#11 slot leakage (Bug 1168425)
  unset NSS_STRICT_SHUTDOWN
  SSLGTESTREPORT="${SSLGTESTDIR}/report.xml"
  ${BINDIR}/ssl_gtest -d "${SSLGTESTDIR}" --gtest_output=xml:"${SSLGTESTREPORT}"
  html_msg $? 0 "ssl_gtest run successfully"
  sed -f ${QADIR}/ssl_gtests/parsereport.sed "${SSLGTESTREPORT}" | \
  while read result name; do
    if [ "$result" = "notrun" ]; then
      echo "$name" SKIPPED
    elif [ "$result" = "run" ]; then
      html_passed "$name" > /dev/null
    else
      html_failed "$name"
    fi
  done
}

ssl_gtest_cleanup()
{
  cd ${QADIR}
  . common/cleanup.sh
}

################## main #################################################
cd "$(dirname "$0")"
ssl_gtest_init
ssl_gtest_start
ssl_gtest_cleanup
