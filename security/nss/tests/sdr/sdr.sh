#! /bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
#
# mozilla/security/nss/tests/sdr/sdr.sh
#
# Script to start test basic functionallity of NSS sdr
#
# needs to work on all Unix and Windows platforms
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
########################################################################

############################## sdr_init ################################
# local shell function to initialize this script
########################################################################
sdr_init()
{
  SCRIPTNAME=sdr.sh
  if [ -z "${CLEANUP}" ] ; then
      CLEANUP="${SCRIPTNAME}"
  fi
  
  if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ]; then
      cd ../common
      . ./init.sh
  fi
  SCRIPTNAME=sdr.sh

  #temporary files
  VALUE1=$HOSTDIR/tests.v1.$$
  VALUE2=$HOSTDIR/tests.v2.$$
  VALUE3=$HOSTDIR/tests.v3.$$

  T1="Test1"
  T2="The quick brown fox jumped over the lazy dog"
  T3="1234567"

  SDRDIR=${HOSTDIR}/SDR
  D_SDR="SDR.$version"
  if [ ! -d ${SDRDIR} ]; then
    mkdir -p ${SDRDIR}
  fi

  PROFILE=.
  if [ -n "${MULTIACCESS_DBM}" ]; then
     PROFILE="multiaccess:${D_SDR}"
  fi

  cd ${SDRDIR}
  html_head "SDR Tests"
}

############################## sdr_main ################################
# local shell function to test NSS SDR
########################################################################
sdr_main()
{
  echo "$SCRIPTNAME: Creating an SDR key/SDR Encrypt - Value 1"
  echo "sdrtest -d ${PROFILE} -o ${VALUE1} -t \"${T1}\""
  ${BINDIR}/sdrtest -d ${PROFILE} -o ${VALUE1} -t "${T1}"
  html_msg $? 0 "Creating SDR Key/Encrypt - Value 1"

  echo "$SCRIPTNAME: SDR Encrypt - Value 2"
  echo "sdrtest -d ${PROFILE} -o ${VALUE2} -t \"${T2}\""
  ${BINDIR}/sdrtest -d ${PROFILE} -o ${VALUE2} -t "${T2}"
  html_msg $? 0 "Encrypt - Value 2"

  echo "$SCRIPTNAME: SDR Encrypt - Value 3"
  echo "sdrtest -d ${PROFILE} -o ${VALUE3} -t \"${T3}\""
  ${BINDIR}/sdrtest -d ${PROFILE} -o ${VALUE3} -t "${T3}"
  html_msg $? 0 "Encrypt - Value 3"

  echo "$SCRIPTNAME: SDR Decrypt - Value 1"
  echo "sdrtest -d ${PROFILE} -i ${VALUE1} -t \"${T1}\""
  ${BINDIR}/sdrtest -d ${PROFILE} -i ${VALUE1} -t "${T1}"
  html_msg $? 0 "Decrypt - Value 1"

  echo "$SCRIPTNAME: SDR Decrypt - Value 2"
  echo "sdrtest -d ${PROFILE} -i ${VALUE2} -t \"${T2}\""
  ${BINDIR}/sdrtest -d ${PROFILE} -i ${VALUE2} -t "${T2}"
  html_msg $? 0 "Decrypt - Value 2"

  echo "$SCRIPTNAME: SDR Decrypt - Value 3"
  echo "sdrtest -d ${PROFILE} -i ${VALUE3} -t \"${T3}\""
  ${BINDIR}/sdrtest -d ${PROFILE} -i ${VALUE3} -t "${T3}"
  html_msg $? 0 "Decrypt - Value 3"
}

############################## sdr_cleanup #############################
# local shell function to finish this script (no exit since it might be
# sourced)
########################################################################
sdr_cleanup()
{
  html "</TABLE><BR>"
  cd ${QADIR}
  . common/cleanup.sh
}

sdr_init
sdr_main
sdr_cleanup
