#! /bin/sh
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Netscape security libraries.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1994-2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

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

  T1="Test1"
  T2="The quick brown fox jumped over the lazy dog"

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
  echo "$SCRIPTNAME: Creating an SDR key/Encrypt"
  echo "sdrtest -d ${PROFILE} -o ${VALUE1} -t Test1"
  sdrtest -d ${PROFILE} -o ${VALUE1} -t Test1
  html_msg $? 0 "Creating SDR Key"

  echo "$SCRIPTNAME: SDR Encrypt - Second Value"
  echo "sdrtest -d ${PROFILE} -o ${VALUE2} -t '${T2}'"
  sdrtest -d ${PROFILE} -o ${VALUE2} -t "${T2}"
  html_msg $? 0 "Encrypt - Value 2"

  echo "$SCRIPTNAME: Decrypt - Value 1"
  echo "sdrtest -d ${PROFILE} -i ${VALUE1} -t Test1"
  sdrtest -d ${PROFILE} -i ${VALUE1} -t Test1
  html_msg $? 0 "Decrypt - Value 1"

  echo "$SCRIPTNAME: Decrypt - Value 2"
  echo "sdrtest -d ${PROFILE} -i ${VALUE2} -t ${T2}"
  sdrtest -d ${PROFILE} -i ${VALUE2} -t "${T2}"
  html_msg $? 0 "Decrypt - Value 2"
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
