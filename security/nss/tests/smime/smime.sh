#! /bin/sh  
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
#
# mozilla/security/nss/tests/smime/smime.sh
#
# Script to test NSS smime
#
# needs to work on all Unix and Windows platforms
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
########################################################################

############################## smime_init ##############################
# local shell function to initialize this script
########################################################################
smime_init()
{
  SCRIPTNAME=smime.sh      # sourced - $0 would point to all.sh

  if [ -z "${CLEANUP}" ] ; then     # if nobody else is responsible for
      CLEANUP="${SCRIPTNAME}"       # cleaning this script will do it
  fi

  if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ]; then
      cd ../common
      . ./init.sh
  fi
  if [ ! -r $CERT_LOG_FILE ]; then  # we need certificates here
      cd ../cert
      . ./cert.sh
  fi
  SCRIPTNAME=smime.sh

  if [ -n "$NSS_ENABLE_ECC" ] ; then
      html_head "S/MIME Tests with ECC"
  else
      html_head "S/MIME Tests"
  fi

  grep "SUCCESS: SMIME passed" $CERT_LOG_FILE >/dev/null || {
      Exit 11 "Fatal - S/MIME of cert.sh needs to pass first"
  }

  SMIMEDIR=${HOSTDIR}/smime
  R_SMIMEDIR=../smime
  mkdir -p ${SMIMEDIR}
  cd ${SMIMEDIR}
  cp ${QADIR}/smime/alice.txt ${SMIMEDIR}
}

smime_sign()
{
  HASH_CMD="-H ${HASH}"
  SIG=sig.${HASH}

  echo "$SCRIPTNAME: Signing Detached Message {$HASH} ------------------"
  echo "cmsutil -S -T -N Alice ${HASH_CMD} -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice.d${SIG}"
  ${PROFTOOL} ${BINDIR}/cmsutil -S -T -N Alice ${HASH_CMD} -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice.d${SIG}
  html_msg $? 0 "Create Detached Signature Alice (${HASH})" "."

  echo "cmsutil -D -i alice.d${SIG} -c alice.txt -d ${P_R_BOBDIR} "
  ${PROFTOOL} ${BINDIR}/cmsutil -D -i alice.d${SIG} -c alice.txt -d ${P_R_BOBDIR} 
  html_msg $? 0 "Verifying Alice's Detached Signature (${HASH})" "."

  echo "$SCRIPTNAME: Signing Attached Message (${HASH}) ------------------"
  echo "cmsutil -S -N Alice ${HASH_CMD} -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice.${SIG}"
  ${PROFTOOL} ${BINDIR}/cmsutil -S -N Alice ${HASH_CMD} -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice.${SIG}
  html_msg $? 0 "Create Attached Signature Alice (${HASH})" "."

  echo "cmsutil -D -i alice.${SIG} -d ${P_R_BOBDIR} -o alice.data.${HASH}"
  ${PROFTOOL} ${BINDIR}/cmsutil -D -i alice.${SIG} -d ${P_R_BOBDIR} -o alice.data.${HASH}
  html_msg $? 0 "Decode Alice's Attached Signature (${HASH})" "."

  echo "diff alice.txt alice.data.${HASH}"
  diff alice.txt alice.data.${HASH}
  html_msg $? 0 "Compare Attached Signed Data and Original (${HASH})" "."

# Test ECDSA signing for all hash algorithms.
  if [ -n "$NSS_ENABLE_ECC" ] ; then
      echo "$SCRIPTNAME: Signing Detached Message ECDSA w/ {$HASH} ------------------"
      echo "cmsutil -S -T -N Alice-ec ${HASH_CMD} -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice-ec.d${SIG}"
      ${PROFTOOL} ${BINDIR}/cmsutil -S -T -N Alice-ec ${HASH_CMD} -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice-ec.d${SIG}
      html_msg $? 0 "Create Detached Signature Alice (ECDSA w/ ${HASH})" "."

      echo "cmsutil -D -i alice-ec.d${SIG} -c alice.txt -d ${P_R_BOBDIR} "
      ${PROFTOOL} ${BINDIR}/cmsutil -D -i alice-ec.d${SIG} -c alice.txt -d ${P_R_BOBDIR} 
      html_msg $? 0 "Verifying Alice's Detached Signature (ECDSA w/ ${HASH})" "."

      echo "$SCRIPTNAME: Signing Attached Message (ECDSA w/ ${HASH}) ------------------"
      echo "cmsutil -S -N Alice-ec ${HASH_CMD} -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice-ec.${SIG}"
      ${PROFTOOL} ${BINDIR}/cmsutil -S -N Alice-ec ${HASH_CMD} -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice-ec.${SIG}
      html_msg $? 0 "Create Attached Signature Alice (ECDSA w/ ${HASH})" "."

      echo "cmsutil -D -i alice-ec.${SIG} -d ${P_R_BOBDIR} -o alice-ec.data.${HASH}"
      ${PROFTOOL} ${BINDIR}/cmsutil -D -i alice-ec.${SIG} -d ${P_R_BOBDIR} -o alice-ec.data.${HASH}
      html_msg $? 0 "Decode Alice's Attached Signature (ECDSA w/ ${HASH})" "."

      echo "diff alice.txt alice-ec.data.${HASH}"
      diff alice.txt alice-ec.data.${HASH}
      html_msg $? 0 "Compare Attached Signed Data and Original (ECDSA w/ ${HASH})" "."
  fi

}



smime_p7()
{
  echo "$SCRIPTNAME: p7 util Data Tests ------------------------------"
  echo "p7env -d ${P_R_ALICEDIR} -r Alice -i alice.txt -o alice_p7.env"
  ${PROFTOOL} ${BINDIR}/p7env -d ${P_R_ALICEDIR} -r Alice -i alice.txt -o alice.env
  html_msg $? 0 "Creating envelope for user Alice" "."

  echo "p7content -d ${P_R_ALICEDIR} -i alice.env -o alice_p7.data"
  ${PROFTOOL} ${BINDIR}/p7content -d ${P_R_ALICEDIR} -i alice.env -o alice_p7.data -p nss
  html_msg $? 0 "Verifying file delivered to user Alice" "."

  sed -e '3,8p' -n alice_p7.data > alice_p7.data.sed

  echo "diff alice.txt alice_p7.data.sed"
  diff alice.txt alice_p7.data.sed
  html_msg $? 0 "Compare Decoded Enveloped Data and Original" "."

  echo "p7sign -d ${P_R_ALICEDIR} -k Alice -i alice.txt -o alice.sig -p nss -e"
  ${PROFTOOL} ${BINDIR}/p7sign -d ${P_R_ALICEDIR} -k Alice -i alice.txt -o alice.sig -p nss -e
  html_msg $? 0 "Signing file for user Alice" "."

  echo "p7verify -d ${P_R_ALICEDIR} -c alice.txt -s alice.sig"
  ${PROFTOOL} ${BINDIR}/p7verify -d ${P_R_ALICEDIR} -c alice.txt -s alice.sig
  html_msg $? 0 "Verifying file delivered to user Alice" "."
}

############################## smime_main ##############################
# local shell function to test basic signed and enveloped messages 
# from 1 --> 2"
########################################################################
smime_main()
{

  HASH=SHA1
  smime_sign
  HASH=SHA256
  smime_sign
  HASH=SHA384
  smime_sign
  HASH=SHA512
  smime_sign

  echo "$SCRIPTNAME: Enveloped Data Tests ------------------------------"
  echo "cmsutil -E -r bob@bogus.com -i alice.txt -d ${P_R_ALICEDIR} -p nss \\"
  echo "        -o alice.env"
  ${PROFTOOL} ${BINDIR}/cmsutil -E -r bob@bogus.com -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice.env
  html_msg $? 0 "Create Enveloped Data Alice" "."

  echo "cmsutil -D -i alice.env -d ${P_R_BOBDIR} -p nss -o alice.data1"
  ${PROFTOOL} ${BINDIR}/cmsutil -D -i alice.env -d ${P_R_BOBDIR} -p nss -o alice.data1
  html_msg $? 0 "Decode Enveloped Data Alice" "."

  echo "diff alice.txt alice.data1"
  diff alice.txt alice.data1
  html_msg $? 0 "Compare Decoded Enveloped Data and Original" "."

  # multiple recip
  echo "$SCRIPTNAME: Testing multiple recipients ------------------------------"
  echo "cmsutil -E -i alice.txt -d ${P_R_ALICEDIR} -o alicecc.env \\"
  echo "        -r bob@bogus.com,dave@bogus.com"
  ${PROFTOOL} ${BINDIR}/cmsutil -E -i alice.txt -d ${P_R_ALICEDIR} -o alicecc.env \
          -r bob@bogus.com,dave@bogus.com
  ret=$?
  html_msg $ret 0 "Create Multiple Recipients Enveloped Data Alice" "."
  if [ $ret != 0 ] ; then
	echo "certutil -L -d ${P_R_ALICEDIR}"
	${BINDIR}/certutil -L -d ${P_R_ALICEDIR}
	echo "certutil -L -d ${P_R_ALICEDIR} -n dave@bogus.com"
	${BINDIR}/certutil -L -d ${P_R_ALICEDIR} -n dave@bogus.com
  fi

  echo "$SCRIPTNAME: Testing multiple email addrs ------------------------------"
  echo "cmsutil -E -i alice.txt -d ${P_R_ALICEDIR} -o aliceve.env \\"
  echo "        -r eve@bogus.net"
  ${PROFTOOL} ${BINDIR}/cmsutil -E -i alice.txt -d ${P_R_ALICEDIR} -o aliceve.env \
          -r eve@bogus.net
  ret=$?
  html_msg $ret 0 "Encrypt to a Multiple Email cert" "."

  echo "cmsutil -D -i alicecc.env -d ${P_R_BOBDIR} -p nss -o alice.data2"
  ${PROFTOOL} ${BINDIR}/cmsutil -D -i alicecc.env -d ${P_R_BOBDIR} -p nss -o alice.data2
  html_msg $? 0 "Decode Multiple Recipients Enveloped Data Alice by Bob" "."

  echo "cmsutil -D -i alicecc.env -d ${P_R_DAVEDIR} -p nss -o alice.data3"
  ${PROFTOOL} ${BINDIR}/cmsutil -D -i alicecc.env -d ${P_R_DAVEDIR} -p nss -o alice.data3
  html_msg $? 0 "Decode Multiple Recipients Enveloped Data Alice by Dave" "."

  echo "cmsutil -D -i aliceve.env -d ${P_R_EVEDIR} -p nss -o alice.data4"
  ${PROFTOOL} ${BINDIR}/cmsutil -D -i aliceve.env -d ${P_R_EVEDIR} -p nss -o alice.data4
  html_msg $? 0 "Decrypt with a Multiple Email cert" "."

  diff alice.txt alice.data2
  html_msg $? 0 "Compare Decoded Mult. Recipients Enveloped Data Alice/Bob" "."

  diff alice.txt alice.data3
  html_msg $? 0 "Compare Decoded Mult. Recipients Enveloped Data Alice/Dave" "."

  diff alice.txt alice.data4
  html_msg $? 0 "Compare Decoded with Multiple Email cert" "."
  
  echo "$SCRIPTNAME: Sending CERTS-ONLY Message ------------------------------"
  echo "cmsutil -O -r \"Alice,bob@bogus.com,dave@bogus.com\" \\"
  echo "        -d ${P_R_ALICEDIR} > co.der"
  ${PROFTOOL} ${BINDIR}/cmsutil -O -r "Alice,bob@bogus.com,dave@bogus.com" -d ${P_R_ALICEDIR} > co.der
  html_msg $? 0 "Create Certs-Only Alice" "."

  echo "cmsutil -D -i co.der -d ${P_R_BOBDIR}"
  ${PROFTOOL} ${BINDIR}/cmsutil -D -i co.der -d ${P_R_BOBDIR}
  html_msg $? 0 "Verify Certs-Only by CA" "."

  echo "$SCRIPTNAME: Encrypted-Data Message ---------------------------------"
  echo "cmsutil -C -i alice.txt -e alicehello.env -d ${P_R_ALICEDIR} \\"
  echo "        -r \"bob@bogus.com\" > alice.enc"
  ${PROFTOOL} ${BINDIR}/cmsutil -C -i alice.txt -e alicehello.env -d ${P_R_ALICEDIR} \
          -r "bob@bogus.com" > alice.enc
  html_msg $? 0 "Create Encrypted-Data" "."

  echo "cmsutil -D -i alice.enc -d ${P_R_BOBDIR} -e alicehello.env -p nss \\"
  echo "        -o alice.data2"
  ${PROFTOOL} ${BINDIR}/cmsutil -D -i alice.enc -d ${P_R_BOBDIR} -e alicehello.env -p nss -o alice.data2
  html_msg $? 0 "Decode Encrypted-Data" "."

  diff alice.txt alice.data2
  html_msg $? 0 "Compare Decoded and Original Data" "."
}
  
############################## smime_cleanup ###########################
# local shell function to finish this script (no exit since it might be
# sourced)
########################################################################
smime_cleanup()
{
  html "</TABLE><BR>"
  cd ${QADIR}
  . common/cleanup.sh
}

################## main #################################################

smime_init
smime_main
smime_p7
smime_cleanup

