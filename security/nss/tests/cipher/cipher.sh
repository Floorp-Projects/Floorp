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
  GCMTESTDIR=${QADIR}/../cmd/pk11gcmtest
  D_CIPHER="Cipher.$version"

  CIPHER_TXT=${QADIR}/cipher/cipher.txt
  GCM_TXT=${QADIR}/cipher/gcm.txt

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
                 ${PROFTOOL} ${BINDIR}/bltest${PROG_SUFFIX} -T -m $PARAM -d $CIPHERTESTDIR -1 $inOff -2 $outOff
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

############################## cipher_gcm #############################
# local shell function to test NSS AES GCM
########################################################################
cipher_gcm()
{
  while read EXP_RET INPUT_FILE TESTNAME
  do
      if [ -n "$EXP_RET" -a "$EXP_RET" != "#" ] ; then
          TESTNAME=`echo $TESTNAME | sed -e "s/_/ /g"`
          echo "$SCRIPTNAME: $TESTNAME --------------------------------"
          echo "pk11gcmtest aes kat gcm $GCMTESTDIR/tests/$INPUT_FILE"
          ${PROFTOOL} ${BINDIR}/pk11gcmtest aes kat gcm $GCMTESTDIR/tests/$INPUT_FILE
          html_msg $? $EXP_RET "$TESTNAME"
      fi
  done < ${GCM_TXT}
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

# When building without softoken, bltest isn't built. It was already
# built and the cipher suite run as part of an nss-softoken build. 
if [ ! -x ${DIST}/${OBJDIR}/bin/bltest${PROG_SUFFIX} ]; then
    echo "bltest not built, skipping this test." >> ${LOGFILE}
    res = 0
    html_msg $res $EXP_RET "$TESTNAME"
    return 0
fi
cipher_init
# Skip cipher_main if this an NSS without softoken build.
if [ "${NSS_BUILD_WITHOUT_SOFTOKEN}" != "1" ]; then
    cipher_main
fi
# Skip cipher_gcm if this is a softoken only build.
if [ "${NSS_BUILD_SOFTOKEN_ONLY}" != "1" ]; then
    cipher_gcm
fi
cipher_cleanup
