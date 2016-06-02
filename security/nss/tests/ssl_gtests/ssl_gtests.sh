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

# Generate input to certutil
certscript() {
  while [ $# -gt 0 ]; do
    case $1 in
      sign) echo 0 ;;
      kex) echo 2 ;;
    esac; shift
  done;
  echo 9
  echo n
  echo n
  echo
  echo n
}

# $1: name
# $2: type
# $3+: usages: sign or kex
make_cert() {
  name=$1
  type=$2
  case $type in
    dsa) type_args='-g 1024' ;;
    rsa) type_args='-g 1024' ;;
    ec) type_args='-q nistp256' ;;
  esac
  shift 2
  certscript $@ | ${BINDIR}/certutil -S \
    -z ${R_NOISE_FILE} -d "${PROFILEDIR}" \
    -n $name -s "CN=$name" -t C,C,C -x -m 1 -w -2 -v 120 \
    -k $type $type_args -Z SHA256 -1 -2
  html_msg $? 0 "create certificate: $@"
}

ssl_gtest_certs() {
  mkdir -p "${SSLGTESTDIR}"
  cd "${SSLGTESTDIR}"

  PROFILEDIR=`pwd`
  if [ "${OS_ARCH}" = "WINNT" -a "$OS_NAME" = "CYGWIN_NT" ]; then
    PROFILEDIR=`cygpath -m "${PROFILEDIR}"`
  fi

  ${BINDIR}/certutil -N -d "${PROFILEDIR}" --empty-password 2>&1
  html_msg $? 0 "create ssl_gtest database"

  make_cert client rsa sign
  # Server certs are named by type
  make_cert rsa rsa sign kex
  make_cert rsa_sign rsa sign
  make_cert rsa_decrypt rsa kex
  make_cert ecdsa ec sign
  make_cert ecdh_ecdsa ec kex
  # TODO ecdh_rsa
  make_cert dsa dsa sign
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

  SSLGTESTREPORT="${SSLGTESTDIR}/report.xml"
  ${BINDIR}/ssl_gtest -d "${SSLGTESTDIR}" --gtest_output=xml:"${SSLGTESTREPORT}"
  html_msg $? 0 "ssl_gtest run successfully"
  sed -f ${COMMON}/parsegtestreport.sed "${SSLGTESTREPORT}" | \
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
