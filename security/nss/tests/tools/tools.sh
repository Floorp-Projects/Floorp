#! /bin/sh  
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
#
#
########################################################################
#
# mozilla/security/nss/tests/tools/tools.sh
#
# Script to test basic functionallity of NSS tools 
#
# needs to work on all Unix and Windows platforms
#
# tests implemented:
#    pk12util
#    signtool
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
########################################################################

############################## tools_init ##############################
# local shell function to initialize this script 
########################################################################
tools_init()
{
  SCRIPTNAME=tools.sh      # sourced - $0 would point to all.sh

  if [ -z "${CLEANUP}" ] ; then     # if nobody else is responsible for
      CLEANUP="${SCRIPTNAME}"       # cleaning this script will do it
  fi

  if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ]; then
      cd ../common
      . init.sh
  fi
  if [ ! -r $CERT_LOG_FILE ]; then  # we need certificates here
      cd ../cert
      . cert.sh
  fi
  SCRIPTNAME=tools.sh
  html_head "Tools Tests"

  grep "SUCCESS: SMIME passed" $CERT_LOG_FILE >/dev/null || {
      Exit 15 "Fatal - S/MIME of cert.sh needs to pass first"
  }

  TOOLSDIR=${HOSTDIR}/tools
  COPYDIR=${TOOLSDIR}/copydir

  R_TOOLSDIR=../tools
  R_COPYDIR=../tools/copydir

  mkdir -p ${TOOLSDIR}
  mkdir -p ${COPYDIR}
  mkdir -p ${TOOLSDIR}/html
  cp ${QADIR}/tools/sign*.html ${TOOLSDIR}/html

  cd ${TOOLSDIR}
}

############################## tools_p12 ###############################
# local shell function to test basic functionality of pk12util
########################################################################
tools_p12()
{
  echo "$SCRIPTNAME: Exporting Alice's email cert & key------------------"
  echo "pk12util -o Alice.p12 -n \"Alice\" -d ${R_ALICEDIR} -k ${R_PWFILE} \\"
  echo "         -w ${R_PWFILE}"
  pk12util -o Alice.p12 -n "Alice" -d ${R_ALICEDIR} -k ${R_PWFILE} \
           -w ${R_PWFILE} 2>&1 
  ret=$?
  html_msg $ret 0 "Exporting Alice's email cert & key (pk12util -o)"
  check_tmpfile

  echo "$SCRIPTNAME: Importing Alice's email cert & key -----------------"
  echo "pk12util -i Alice.p12 -d ${R_COPYDIR} -k ${R_PWFILE} -w ${R_PWFILE}"
  pk12util -i Alice.p12 -d ${R_COPYDIR} -k ${R_PWFILE} -w ${R_PWFILE} 2>&1
  ret=$?
  html_msg $ret 0 "Importing Alice's email cert & key (pk12util -i)"
  check_tmpfile
}

############################## tools_sign ##############################
# local shell function pk12util uses a hardcoded tmp file, if this exists
# and is owned by another user we don't get reasonable errormessages 
########################################################################
check_tmpfile()
{
  if [ $ret != "0" -a -f /tmp/Pk12uTemp ] ; then
      echo "Error: pk12util temp file exists. Please remove this file and"
      echo "       rerun the test (/tmp/Pk12uTemp) "
  fi
}

############################## tools_sign ##############################
# local shell function to test basic functionality of signtool
########################################################################
tools_sign()
{
  echo "$SCRIPTNAME: Create objsign cert -------------------------------"
  echo "signtool -G \"objectsigner\" -d ${R_ALICEDIR} -p \"nss\""
  signtool -G "objsigner" -d ${R_ALICEDIR} -p "nss" 2>&1 <<SIGNSCRIPT
y
TEST
MOZ
NSS
NY
US
liz
liz@moz.org
SIGNSCRIPT
  html_msg $? 0 "Create objsign cert (signtool -G)"

  echo "$SCRIPTNAME: Signing a set of files ----------------------------"
  echo "signtool -Z nojs.jar -d ${R_ALICEDIR} -p \"nss\" -k objsigner \\"
  echo "         ${R_TOOLSDIR}/html"
  signtool -Z nojs.jar -d ${R_ALICEDIR} -p "nss" -k objsigner ${R_TOOLSDIR}/html
  html_msg $? 0 "Signing a set of files (signtool -Z)"

  echo "$SCRIPTNAME: Listing signed files in jar ----------------------"
  echo "signtool -v nojs.jar -d ${R_ALICEDIR} -p nss -k objsigner"
  signtool -v nojs.jar -d ${R_ALICEDIR} -p nss -k objsigner
  html_msg $? 0 "Listing signed files in jar (signtool -v)"
  
  echo "$SCRIPTNAME: Show who signed jar ------------------------------"
  echo "signtool -w nojs.jar -d ${R_ALICEDIR}"
  signtool -w nojs.jar -d ${R_ALICEDIR}
  html_msg $? 0 "Show who signed jar (signtool -w)"
}

############################## tools_cleanup ###########################
# local shell function to finish this script (no exit since it might be 
# sourced)
########################################################################
tools_cleanup()
{
  html "</TABLE><BR>"
  cd ${QADIR}
  . common/cleanup.sh
}

################## main #################################################

tools_init

tools_p12

tools_sign
tools_cleanup


