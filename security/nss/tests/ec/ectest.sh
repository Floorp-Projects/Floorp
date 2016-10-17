#! /bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
#
# tests/ec/ectest.sh
#
# needs to work on all Unix and Windows platforms
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
########################################################################

############################## ectest_init #############################
# local shell function to initialize this script
########################################################################

ectest_init()
{
  SCRIPTNAME="ectest.sh"
  if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ] ; then
      cd ../common
      . ./init.sh
  fi
  SCRIPTNAME="ectest.sh"
  html_head "freebl and pk11 ectest tests"
}

ectest_cleanup()
{
  html "</TABLE><BR>"
  cd ${QADIR}
  . common/cleanup.sh
}

ectest_genkeydb_test()
{
  certutil -N -d "${HOSTDIR}" -f "${R_PWFILE}" 2>&1
  if [ $? -ne 0 ]; then
    return $?
  fi
  curves=( \
    "curve25519" \
    "secp256r1" \
    "secp384r1" \
    "secp521r1" \
  )
  for curve in "${curves[@]}"; do
    echo "Test $curve key generation using certutil ..."
    certutil -G -d "${HOSTDIR}" -k ec -q $curve -f "${R_PWFILE}" -z ${NOISE_FILE}
    if [ $? -ne 0 ]; then
      html_failed "ec test certutil keygen - $curve"
    else
      html_passed "ec test certutil keygen - $curve"
    fi
  done
  echo "Test sect571r1 key generation using certutil that should fail because it's not implemented ..."
  certutil -G -d "${HOSTDIR}" -k ec -q sect571r1 -f "${R_PWFILE}" -z ${NOISE_FILE}
  if [ $? -eq 0 ]; then
    html_failed "ec test certutil keygen - $curve"
  else
    html_passed "ec test certutil keygen - $curve"
  fi
}

ectest_init
ectest_genkeydb_test
# TODO: expose individual tests and failures instead of overall
if [ -f ${BINDIR}/fbectest ]; then
  FB_ECTEST_OUT=$(fbectest -n -d 2>&1)
  FB_ECTEST_OUT=`echo $FB_ECTEST_OUT | grep -i 'not okay\|Assertion failure'`
  if [ -n "$FB_ECTEST_OUT" ] ; then
    html_failed "freebl ec tests"
  else
    html_passed "freebl ec tests"
  fi
fi
if [ -f ${BINDIR}/pk11ectest ]; then
  PK11_ECTEST_OUT=$(pk11ectest -n -d 2>&1)
  PK11_ECTEST_OUT=`echo $PK11_ECTEST_OUT | grep -i 'not okay\|Assertion failure'`
  if [ -n "$PK11_ECTEST_OUT" ] ; then
    html_failed "pk11 ec tests"
  else
    html_passed "pk11 ec tests"
  fi
fi
ectest_cleanup
