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
# mozilla/security/nss/tests/cipher/cipher.sh
#
# Script to test NSS ciphers
#
# needs to work on all Unix and Windows platforms
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
########################################################################

############################## cipher_init #############################
# local shell function to initialize this script
########################################################################
cipher_init()
{
  SCRIPTNAME="cipher.sh"
  if [ -z "${CLEANUP}" ] ; then     # if nobody else is responsible for
      CLEANUP="${SCRIPTNAME}"       # cleaning this script will do it
  fi
  if [ -z "${INIT_SOURCED}" ] ; then
      cd ../common
      . ./init.sh
  fi
  SCRIPTNAME="cipher.sh"
  html_head "Cipher Tests"

  CIPHERDIR=${HOSTDIR}/cipher
  CIPHERTESTDIR=${QADIR}/../cmd/bltest
  D_CIPHER="Cipher.$version"

  CIPHER_TXT=${QADIR}/cipher/cipher.txt

  mkdir -p ${CIPHERDIR}

  cd ${CIPHERTESTDIR}
  P_CIPHER=.
  if [ -n "${MULTIACCESS_DBM}" ]; then
    P_CIPHER="multiaccess:${D_CIPHER}"
  fi
}

############################## cipher_main #############################
# local shell function to test NSS ciphers
########################################################################
cipher_main()
{
  cat ${CIPHER_TXT} | while read EXP_RET PARAM TESTNAME
  do
      if [ -n "$EXP_RET" -a "$EXP_RET" != "#" ] ; then
          PARAM=`echo $PARAM | sed -e "s/_-/ -/g"`
          TESTNAME=`echo $TESTNAME | sed -e "s/_/ /g"`
          echo "$SCRIPTNAME: $TESTNAME --------------------------------"
          echo "bltest -T -m $PARAM -d ${P_CIPHER}"

          bltest -T -m $PARAM -d ${P_CIPHER} 
          html_msg $? $EXP_RET "$TESTNAME"
      fi
  done
}

############################## cipher_cleanup ############################
# local shell function to finish this script (no exit since it might be
# sourced)
########################################################################
cipher_cleanup()
{
  html "</TABLE><BR>"
  cd ${QADIR}
  . common/cleanup.sh
}

################## main #################################################

cipher_init
cipher_main
cipher_cleanup
