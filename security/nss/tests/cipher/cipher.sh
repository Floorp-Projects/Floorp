#! /bin/bash  
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

  cd ${CIPHERDIR}
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
  while read EXP_RET PARAM TESTNAME
  do
      if [ -n "$EXP_RET" -a "$EXP_RET" != "#" ] ; then
          PARAM=`echo $PARAM | sed -e "s/_-/ -/g"`
          TESTNAME=`echo $TESTNAME | sed -e "s/_/ /g"`
          echo "$SCRIPTNAME: $TESTNAME --------------------------------"
          failedStr=""
          inOff=0
          res=0
          while [ $inOff -lt 8 ]
          do
             outOff=0
             while [ $outOff -lt 8 ]
             do
                 echo "bltest -T -m $PARAM -d $CIPHERTESTDIR -1 $inOff -2 $outOff"
                 ${PROFTOOL} ${BINDIR}/bltest -T -m $PARAM -d $CIPHERTESTDIR -1 $inOff -2 $outOff
                 if [ $? -ne 0 ]; then
                     failedStr="$failedStr[$inOff:$outOff]"
                 fi
                 outOff=`expr $outOff + 1`
             done
             inOff=`expr $inOff + 1`
          done
          if [ -n "$failedStr" ]; then
              html_msg 1 $EXP_RET "$TESTNAME (Failed in/out offset pairs:" \
                        " $failedStr)"
          else
              html_msg $res $EXP_RET "$TESTNAME"
          fi
      fi
  done < ${CIPHER_TXT}
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
