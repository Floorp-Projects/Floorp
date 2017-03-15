#!/bin/bash
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
      ca) echo 5;echo 6 ;;
    esac; shift
  done;
  echo 9
  echo n
  echo ${ca:-n}
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
    rsa2048) type_args='-g 2048';type=rsa ;;
    rsapss) type_args='-g 1024 --pss';type=rsa ;;
    p256) type_args='-q nistp256';type=ec ;;
    p384) type_args='-q secp384r1';type=ec ;;
    p521) type_args='-q secp521r1';type=ec ;;
    rsa_ca) type_args='-g 1024';trust='CT,CT,CT';ca=y;type=rsa ;;
    rsa_chain) type_args='-g 1024';sign='-c rsa_ca';type=rsa;;
    ecdh_rsa) type_args='-q nistp256';sign='-c rsa_ca';type=ec ;;
  esac
  shift 2
  counter=$(($counter + 1))
  certscript $@ | ${BINDIR}/certutil -S \
    -z ${R_NOISE_FILE} -d "${PROFILEDIR}" \
    -n $name -s "CN=$name" -t ${trust:-,,} ${sign:--x} -m $counter \
    -w -2 -v 120 -k $type $type_args -Z SHA256 -1 -2
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

  counter=0
  make_cert client rsa sign
  make_cert rsa rsa sign kex
  make_cert rsa2048 rsa2048 sign kex
  make_cert rsa_sign rsa sign
  make_cert rsa_pss rsapss sign
  make_cert rsa_decrypt rsa kex
  make_cert ecdsa256 p256 sign
  make_cert ecdsa384 p384 sign
  make_cert ecdsa521 p521 sign
  make_cert ecdh_ecdsa p256 kex
  make_cert rsa_ca rsa_ca ca
  make_cert rsa_chain rsa_chain sign
  make_cert ecdh_rsa ecdh_rsa kex
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

  local nshards=1
  local prefix=""
  local postfix=""

  export -f parallel_fallback

  # Determine the number of chunks.
  if [ -n "$GTESTFILTER" ]; then
    echo "DEBUG: Not parallelizing ssl_gtests because \$GTESTFILTER is set"
  elif type parallel 2>/dev/null; then
    nshards=$(parallel --number-of-cores || 1)
  fi

  if [ "$nshards" != 1 ]; then
    local indices=$(for ((i=0; i<$nshards; i++)); do echo $i; done)
    prefix="parallel -j$nshards --line-buffer --halt soon,fail=1"
    postfix="\&\& exit 0 \|\| exit 1 ::: $indices"
  fi

  echo "DEBUG: ssl_gtests will be divided into $nshards chunk(s)"

  # Run tests.
  ${prefix:-parallel_fallback} \
    GTEST_SHARD_INDEX={} \
      GTEST_TOTAL_SHARDS=$nshards \
        DYLD_LIBRARY_PATH="${DIST}/${OBJDIR}/lib" \
          ${BINDIR}/ssl_gtest -d "${SSLGTESTDIR}" \
            --gtest_output=xml:"${SSLGTESTREPORT}.{}" \
            --gtest_filter="${GTESTFILTER-*}" \
            $postfix

  html_msg $? 0 "ssl_gtests ran successfully"

  # Parse XML report(s).
  if type xmllint &>/dev/null; then
    echo "DEBUG: Using xmllint to parse GTest XML report(s)"
    parse_report
  else
    echo "DEBUG: Falling back to legacy XML report parsing using only sed"
    parse_report_legacy
  fi
}

# Helper function used when 'parallel' isn't available.
parallel_fallback()
{
  eval ${*//\{\}/0}
}

parse_report()
{
  # Check XML reports for normal test runs and failures.
  local successes=$(parse_report_xpath "//testcase[@status='run'][count(*)=0]")
  local failures=$(parse_report_xpath "//failure/..")

  # Print all tests that succeeded.
  while read result name; do
    html_passed_ignore_core "$name"
  done <<< "$successes"

  # Print failing tests.
  if [ -n "$failures" ]; then
    printf "\nFAILURES:\n=========\n"

    while read result name; do
      html_failed_ignore_core "$name"
    done <<< "$failures"

    printf "\n"
  fi
}

parse_report_xpath()
{
  # Query the XML report with the given XPath pattern.
  xmllint --xpath "$1" "${SSLGTESTREPORT}".* 2>/dev/null | \
    # Insert newlines to help sed.
    sed $'s/<testcase/\\\n<testcase/g' | \
    # Use sed to parse the report.
    sed -f "${COMMON}/parsegtestreport.sed"
}

# This legacy report parser can't actually detect failures. It always relied
# on the binary's exit code. Print the tests we ran to keep the old behavior.
parse_report_legacy()
{
  while read result name && [ -n "$name" ]; do
    if [ "$result" = "run" ]; then
      html_passed_ignore_core "$name"
    fi
  done <<< "$(sed -f "${COMMON}/parsegtestreport.sed" "${SSLGTESTREPORT}".*)"
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
