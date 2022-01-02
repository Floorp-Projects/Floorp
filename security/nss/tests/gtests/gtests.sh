#! /bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
#
# similar to all.sh this file runs drives gtests.
#
# needs to work on all Unix and Windows platforms
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
########################################################################

############################## gtest_init ##############################
# local shell function to initialize this script
########################################################################
gtest_init()
{
  cd "$(dirname "$1")"
  pwd
  SOURCE_DIR="$PWD"/../..

  if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ]; then
      cd ../common
      . ./init.sh
  fi

  SCRIPTNAME=gtests.sh
  . "${QADIR}"/common/certsetup.sh

  if [ -z "${CLEANUP}" ] ; then   # if nobody else is responsible for
    CLEANUP="${SCRIPTNAME}"       # cleaning this script will do it
  fi

  mkdir -p "${GTESTDIR}"
  cd "${GTESTDIR}"
}

########################## gtest_start #############################
# Local function to actually start the test
####################################################################
gtest_start()
{
  echo "gtests: ${GTESTS}"
  for i in ${GTESTS}; do
    if [ ! -f "${BINDIR}/$i" ]; then
      html_unknown "Skipping $i (not built)"
      continue
    fi
    DIR="${GTESTDIR}/$i"
    html_head "$i"
    if [ ! -d "$DIR" ]; then
      mkdir -p "$DIR"
      echo "${BINDIR}/certutil" -N -d "$DIR" --empty-password 2>&1
      "${BINDIR}/certutil" -N -d "$DIR" --empty-password 2>&1

      PROFILEDIR="$DIR" make_cert dummy p256 sign
    fi
    pushd "$DIR"
    GTESTREPORT="$DIR/report.xml"
    PARSED_REPORT="$DIR/report.parsed"
    # The mozilla::pkix gtests cause an ODR violation that we ignore.
    # See bug 1588567.
    if [ "$i" = "mozpkix_gtest" ]; then
      EXTRA_ASAN_OPTIONS="detect_odr_violation=0"
    fi
    # NSS CI sets a lower max for PBE iterations, otherwise cert.sh
    # is very slow. Unset this maxiumum for softoken_gtest, as it
    # needs to check the default value.
    if [ "$i" = "softoken_gtest" ]; then
      OLD_MAX_PBE_ITERATIONS=$NSS_MAX_MP_PBE_ITERATION_COUNT
      unset NSS_MAX_MP_PBE_ITERATION_COUNT
    fi
    echo "executing $i"
    ASAN_OPTIONS="$ASAN_OPTIONS:$EXTRA_ASAN_OPTIONS" "${BINDIR}/$i" \
                 -s "${SOURCE_DIR}/gtests/$i" \
                 -d "$DIR" -w --gtest_output=xml:"${GTESTREPORT}" \
                              --gtest_filter="${GTESTFILTER:-*}"
    html_msg $? 0 "$i run successfully"
    if [ "$i" = "softoken_gtest" ]; then
      export NSS_MAX_MP_PBE_ITERATION_COUNT=$OLD_MAX_PBE_ITERATIONS
    fi

    echo "test output dir: ${GTESTREPORT}"
    echo "processing the parsed report"
    gtest_parse_report ${GTESTREPORT}
    popd
  done
}

gtest_cleanup()
{
  html "</TABLE><BR>"
  . "${QADIR}"/common/cleanup.sh
}

################## main #################################################
GTESTS="${GTESTS:-certhigh_gtest certdb_gtest der_gtest pk11_gtest util_gtest freebl_gtest softoken_gtest sysinit_gtest smime_gtest mozpkix_gtest}"
gtest_init "$0"
gtest_start
gtest_cleanup
