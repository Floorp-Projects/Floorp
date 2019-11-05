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
    echo "executing $i"
    ASAN_OPTIONS="$ASAN_OPTIONS:$EXTRA_ASAN_OPTIONS" "${BINDIR}/$i" \
                 "${SOURCE_DIR}/gtests/freebl_gtest/kat/Hash_DRBG.rsp" \
                 -d "$DIR" -w --gtest_output=xml:"${GTESTREPORT}" \
                              --gtest_filter="${GTESTFILTER:-*}"
    html_msg $? 0 "$i run successfully"
    echo "test output dir: ${GTESTREPORT}"
    echo "executing sed to parse the xml report"
    sed -f "${COMMON}/parsegtestreport.sed" "$GTESTREPORT" > "$PARSED_REPORT"
    echo "processing the parsed report"
    cat "$PARSED_REPORT" | while read result name; do
      if [ "$result" = "notrun" ]; then
        echo "$name" SKIPPED
      elif [ "$result" = "run" ]; then
        html_passed_ignore_core "$name"
      else
        html_failed_ignore_core "$name"
      fi
    done
    popd
  done
}

gtest_cleanup()
{
  html "</TABLE><BR>"
  . "${QADIR}"/common/cleanup.sh
}

################## main #################################################
GTESTS="${GTESTS:-prng_gtest certhigh_gtest certdb_gtest der_gtest pk11_gtest util_gtest freebl_gtest softoken_gtest sysinit_gtest blake2b_gtest smime_gtest mozpkix_gtest}"
gtest_init "$0"
gtest_start
gtest_cleanup
