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
  html_head "ectest test"
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
ECTEST_OUT=$(ectest -f -p -n -d 2>&1)
ECTEST_OUT=`echo $ECTEST_OUT | grep -i 'not okay\|Assertion failure'`
# TODO: expose individual tests and failures instead of overall
if [ -n "$ECTEST_OUT" ] ; then
  html_failed "ec freebl and pk11 test"
else
  html_passed "ec freebl and pk11 test"
fi
ectest_cleanup
