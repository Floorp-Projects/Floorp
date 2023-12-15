#! /bin/bash
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

EMAILDATE=`date --rfc-email --utc`

# parameter: MIME part boundary
make_multipart()
{
  mp_start="Content-Type: multipart/signed; protocol=\"application/pkcs7-signature\"; micalg=sha-HASHHASH; boundary=\"$1\"

This is a cryptographically signed message in MIME format.

--$1"

  mp_middle="
--$1
Content-Type: application/pkcs7-signature; name=smime.p7s
Content-Transfer-Encoding: base64
Content-Disposition: attachment; filename=smime.p7s
Content-Description: S/MIME Cryptographic Signature
"

  mp_end="--$1--
"
}

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

  html_head "S/MIME Tests"

  grep "SUCCESS: SMIME passed" $CERT_LOG_FILE >/dev/null || {
      Exit 11 "Fatal - S/MIME of cert.sh needs to pass first"
  }

  SMIMEDIR=${HOSTDIR}/smime
  R_SMIMEDIR=../smime
  mkdir -p ${SMIMEDIR}
  cd ${SMIMEDIR}
  cp ${QADIR}/smime/alice.txt ${SMIMEDIR}

  mkdir tb

  make_multipart "------------ms030903020902020502030404"
  multipart_start="$mp_start"
  multipart_middle="$mp_middle"
  multipart_end="$mp_end"

  make_multipart "------------ms010205070902020502030809"
  multipart_start_b2="$mp_start"
  multipart_middle_b2="$mp_middle"
  multipart_end_b2="$mp_end"
}

cms_sign()
{
  HASH_CMD="-H SHA${HASH}"
  SIG=sig.SHA${HASH}

  echo "$SCRIPTNAME: Signing Detached Message {$HASH} ------------------"
  echo "cmsutil -S -G -T -N Alice ${HASH_CMD} -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice.d${SIG}"
  ${PROFTOOL} ${BINDIR}/cmsutil -S -G -T -N Alice ${HASH_CMD} -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice.d${SIG}
  html_msg $? 0 "Create Detached Signature Alice (${HASH})" "."

  echo "cmsutil -D -i alice.d${SIG} -c alice.txt -d ${P_R_BOBDIR} "
  ${PROFTOOL} ${BINDIR}/cmsutil -D -i alice.d${SIG} -c alice.txt -d ${P_R_BOBDIR}
  html_msg $? 0 "Verifying Alice's Detached Signature (${HASH})" "."

  echo "$SCRIPTNAME: Signing Attached Message (${HASH}) ------------------"
  echo "cmsutil -S -G -N Alice ${HASH_CMD} -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice.${SIG}"
  ${PROFTOOL} ${BINDIR}/cmsutil -S -G -N Alice ${HASH_CMD} -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice.${SIG}
  html_msg $? 0 "Create Attached Signature Alice (${HASH})" "."

  echo "cmsutil -D -i alice.${SIG} -d ${P_R_BOBDIR} -o alice.data.${HASH}"
  ${PROFTOOL} ${BINDIR}/cmsutil -D -i alice.${SIG} -d ${P_R_BOBDIR} -o alice.data.${HASH}
  html_msg $? 0 "Decode Alice's Attached Signature (${HASH})" "."

  echo "diff alice.txt alice.data.${HASH}"
  diff alice.txt alice.data.${HASH}
  html_msg $? 0 "Compare Attached Signed Data and Original (${HASH})" "."

# Test ECDSA signing for all hash algorithms.
  echo "$SCRIPTNAME: Signing Detached Message ECDSA w/ {$HASH} ------------------"
  echo "cmsutil -S -G -T -N Alice-ec ${HASH_CMD} -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice-ec.d${SIG}"
  ${PROFTOOL} ${BINDIR}/cmsutil -S -G -T -N Alice-ec ${HASH_CMD} -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice-ec.d${SIG}
  html_msg $? 0 "Create Detached Signature Alice (ECDSA w/ ${HASH})" "."

  echo "cmsutil -D -i alice-ec.d${SIG} -c alice.txt -d ${P_R_BOBDIR} "
  ${PROFTOOL} ${BINDIR}/cmsutil -D -i alice-ec.d${SIG} -c alice.txt -d ${P_R_BOBDIR}
  html_msg $? 0 "Verifying Alice's Detached Signature (ECDSA w/ ${HASH})" "."

  echo "$SCRIPTNAME: Signing Attached Message (ECDSA w/ ${HASH}) ------------------"
  echo "cmsutil -S -G -N Alice-ec ${HASH_CMD} -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice-ec.${SIG}"
  ${PROFTOOL} ${BINDIR}/cmsutil -S -G -N Alice-ec ${HASH_CMD} -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice-ec.${SIG}
  html_msg $? 0 "Create Attached Signature Alice (ECDSA w/ ${HASH})" "."

  echo "cmsutil -D -i alice-ec.${SIG} -d ${P_R_BOBDIR} -o alice-ec.data.${HASH}"
  ${PROFTOOL} ${BINDIR}/cmsutil -D -i alice-ec.${SIG} -d ${P_R_BOBDIR} -o alice-ec.data.${HASH}
  html_msg $? 0 "Decode Alice's Attached Signature (ECDSA w/ ${HASH})" "."

  echo "diff alice.txt alice-ec.data.${HASH}"
  diff alice.txt alice-ec.data.${HASH}
  html_msg $? 0 "Compare Attached Signed Data and Original (ECDSA w/ ${HASH})" "."
}

header_mime_from_to_subject="MIME-Version: 1.0
Date: ${EMAILDATE}
From: Alice@example.com
To: Bob@example.com
Subject: "

header_dave_mime_from_to_subject="MIME-Version: 1.0
Date: ${EMAILDATE}
From: Dave@example.com
To: Bob@example.com
Subject: "

header_opaque_signed="Content-Type: application/pkcs7-mime; name=smime.p7m;
    smime-type=signed-data
Content-Transfer-Encoding: base64
Content-Disposition: attachment; filename=smime.p7m
Content-Description: S/MIME Cryptographic Signature
"

header_enveloped="Content-Type: application/pkcs7-mime; name=smime.p7m;
    smime-type=enveloped-data
Content-Transfer-Encoding: base64
Content-Disposition: attachment; filename=smime.p7m
Content-Description: S/MIME Encrypted Message
"

header_clearsigned="Content-Type: text/plain; charset=utf-8; format=flowed
Content-Transfer-Encoding: quoted-printable
Content-Language: en-US
"

header_plaintext="Content-Type: text/plain
"

CR=$(printf '\r')

mime_init()
{
  OUT="tb/alice.mime"
  echo "${header_clearsigned}" >>${OUT}
  cat alice.txt >>${OUT}
  sed -i"" "s/\$/${CR}/" ${OUT}

  OUT="tb/alice.textplain"
  echo "${header_plaintext}" >>${OUT}
  cat alice.txt >>${OUT}
  sed -i"" "s/\$/${CR}/" ${OUT}
}

smime_enveloped()
{
  ${PROFTOOL} ${BINDIR}/cmsutil -E -r bob@example.com -i tb/alice.mime -d ${P_R_ALICEDIR} -p nss -o tb/alice.mime.env

  OUT="tb/alice.env"
  echo "${header_enveloped}" >>${OUT}
  cat "tb/alice.mime.env" | ${BINDIR}/btoa | sed 's/\r$//' >>${OUT}
  echo >>${OUT}

  OUT="tb/alice.env.eml"
  echo -n "${header_mime_from_to_subject}" >>${OUT}
  echo "enveloped ${SIG}" >>${OUT}
  cat "tb/alice.env" >>${OUT}
  sed -i"" "s/\$/${CR}/" ${OUT}
}

smime_signed_enveloped()
{
  SIG=sig.SHA${HASH}

  ${PROFTOOL} ${BINDIR}/cmsutil -S -G -T -N Alice ${HASH_CMD} -i tb/alice.mime -d ${P_R_ALICEDIR} -p nss -o tb/alice.mime.d${SIG}

  OUT="tb/alice.d${SIG}.multipart"
  echo "${multipart_start}" | sed "s/HASHHASH/${HASH}/" >>${OUT}
  cat tb/alice.mime | sed 's/\r$//' >>${OUT}
  echo "${multipart_middle}" >>${OUT}
  cat tb/alice.mime.d${SIG} | ${BINDIR}/btoa | sed 's/\r$//' >>${OUT}
  echo "${multipart_end}" >>${OUT}

  ${PROFTOOL} ${BINDIR}/cmsutil -E -r bob@example.com -i ${OUT} -d ${P_R_ALICEDIR} -p nss -o ${OUT}.env

  OUT="tb/alice.d${SIG}.multipart.eml"
  echo -n "${header_mime_from_to_subject}" >>${OUT}
  echo "clear-signed ${SIG}" >>${OUT}
  cat "tb/alice.d${SIG}.multipart" >>${OUT}
  sed -i"" "s/\$/$CR/" ${OUT}

  OUT="tb/alice.d${SIG}.multipart.env.eml"
  echo -n "${header_mime_from_to_subject}" >>${OUT}
  echo "clear-signed then enveloped $SIG" >>${OUT}
  echo "$header_enveloped" >>${OUT}
  cat "tb/alice.d${SIG}.multipart.env" | ${BINDIR}/btoa | sed 's/\r$//' >>${OUT}
  echo >>${OUT}
  sed -i"" "s/\$/$CR/" ${OUT}

  ${PROFTOOL} ${BINDIR}/cmsutil -S -G -N Alice ${HASH_CMD} -i tb/alice.textplain -d ${P_R_ALICEDIR} -p nss -o tb/alice.textplain.${SIG}

  OUT="tb/alice.${SIG}.opaque"
  echo "$header_opaque_signed" >>${OUT}
  cat tb/alice.textplain.${SIG} | ${BINDIR}/btoa | sed 's/\r$//' >>${OUT}

  ${PROFTOOL} ${BINDIR}/cmsutil -E -r bob@example.com -i ${OUT} -d ${P_R_ALICEDIR} -p nss -o ${OUT}.env

  OUT="tb/alice.${SIG}.opaque.eml"
  echo -n "${header_mime_from_to_subject}" >>${OUT}
  echo "opaque-signed $SIG" >>${OUT}
  cat "tb/alice.${SIG}.opaque" >>${OUT}
  echo >>${OUT}
  sed -i"" "s/\$/$CR/" ${OUT}

  OUT="tb/alice.${SIG}.opaque.env.eml"
  echo -n "${header_mime_from_to_subject}" >>${OUT}
  echo "opaque-signed then enveloped $SIG" >>${OUT}
  echo "$header_enveloped" >>$OUT
  cat "tb/alice.${SIG}.opaque.env" | ${BINDIR}/btoa | sed 's/\r$//' >>${OUT}
  echo >>${OUT}
  sed -i"" "s/\$/$CR/" ${OUT}

  # bad messages below

  OUT="tb/alice.d${SIG}.multipart.bad.eml"
  echo -n "${header_mime_from_to_subject}" >>${OUT}
  echo "BAD clear-signed $SIG" >>${OUT}
  cat "tb/alice.d${SIG}.multipart" | sed 's/test message from Alice/FAKE message NOT from Alice/' >>${OUT}
  sed -i"" "s/\$/$CR/" ${OUT}

  OUT="tb/alice.d${SIG}.multipart.mismatch-econtent"
  echo "${multipart_start}" | sed "s/HASHHASH/$HASH/" >>${OUT}
  cat tb/alice.mime | sed 's/test message from Alice/FAKE message NOT from Alice/' | sed 's/\r$//' >>${OUT}
  echo "${multipart_middle}" >>${OUT}
  cat tb/alice.textplain.${SIG} | ${BINDIR}/btoa | sed 's/\r$//' >>${OUT}
  echo "${multipart_end}" >>${OUT}

  OUT="tb/alice.d${SIG}.multipart.mismatch-econtent.eml"
  echo -n "${header_mime_from_to_subject}" >>${OUT}
  echo "BAD mismatch-econtent $SIG" >>${OUT}
  cat "tb/alice.d${SIG}.multipart.mismatch-econtent" >>${OUT}
  sed -i"" "s/\$/$CR/" ${OUT}
}

smime_plain_signed()
{
  SIG=sig.SHA${HASH}

  ${PROFTOOL} ${BINDIR}/cmsutil -S -G -T -N Alice ${HASH_CMD} -i tb/alice.textplain -d ${P_R_ALICEDIR} -p nss -o tb/alice.plain.d${SIG}

  OUT="tb/alice.plain.d${SIG}.multipart"
  echo "${multipart_start}" | sed "s/HASHHASH/${HASH}/" >>${OUT}
  cat tb/alice.textplain | sed 's/\r$//' >>${OUT}
  echo "${multipart_middle}" >>${OUT}
  cat tb/alice.plain.d${SIG} | ${BINDIR}/btoa | sed 's/\r$//' >>${OUT}
  echo "${multipart_end}" >>${OUT}

  ${PROFTOOL} ${BINDIR}/cmsutil -S -G -N Alice ${HASH_CMD} -i tb/alice.textplain -d ${P_R_ALICEDIR} -p nss -o tb/alice.plain.${SIG}

  OUT="tb/alice.plain.${SIG}.opaque"
  echo "$header_opaque_signed" >>${OUT}
  cat tb/alice.plain.${SIG} | ${BINDIR}/btoa | sed 's/\r$//' >>${OUT}

  # Second outer, opaque signature layer.

  INPUT="tb/alice.plain.d${SIG}.multipart"
  OUT_SIG="${INPUT}.dave.${SIG}"
  ${PROFTOOL} ${BINDIR}/cmsutil -S -G -N Dave ${HASH_CMD} -i "$INPUT" -d ${P_R_DAVEDIR} -p nss -o "$OUT_SIG"

  OUT_MIME="${OUT_SIG}.opaque"
  echo "$header_opaque_signed" >>${OUT_MIME}
  cat "$OUT_SIG" | ${BINDIR}/btoa | sed 's/\r$//' >>${OUT_MIME}

  OUT_EML="${OUT_MIME}.eml"
  echo -n "${header_dave_mime_from_to_subject}" >>${OUT_EML}
  echo "clear-signed $SIG then opaque signed by dave" >>${OUT_EML}
  cat "${OUT_MIME}" >>${OUT_EML}
  echo >>${OUT_EML}
  sed -i"" "s/\$/$CR/" ${OUT_EML}

  INPUT="tb/alice.plain.${SIG}.opaque"
  OUT_SIG="${INPUT}.dave.${SIG}"
  ${PROFTOOL} ${BINDIR}/cmsutil -S -G -N Dave ${HASH_CMD} -i "$INPUT" -d ${P_R_DAVEDIR} -p nss -o "$OUT_SIG"

  OUT_MIME="${OUT_SIG}.opaque"
  echo "$header_opaque_signed" >>${OUT_MIME}
  cat "$OUT_SIG" | ${BINDIR}/btoa | sed 's/\r$//' >>${OUT_MIME}

  OUT_EML="${OUT_MIME}.eml"
  echo -n "${header_dave_mime_from_to_subject}" >>${OUT_EML}
  echo "opaque-signed $SIG then opaque signed by dave" >>${OUT_EML}
  cat "${OUT_MIME}" >>${OUT_EML}
  echo >>${OUT_EML}
  sed -i"" "s/\$/$CR/" ${OUT_EML}

  # Alternatively, second outer, multipart signature layer.

  INPUT="tb/alice.plain.d${SIG}.multipart"
  OUT_SIG="${INPUT}.dave.d${SIG}"
  cat "$INPUT" | sed "s/\$/$CR/" > "${INPUT}.cr"
  ${PROFTOOL} ${BINDIR}/cmsutil -S -G -T -N Dave ${HASH_CMD} -i "${INPUT}.cr" -d ${P_R_DAVEDIR} -p nss -o "$OUT_SIG"

  OUT_MIME="${OUT_SIG}.multipart"
  echo "${multipart_start_b2}" | sed "s/HASHHASH/${HASH}/" >>${OUT_MIME}
  cat "${INPUT}.cr" | sed 's/\r$//' >>${OUT_MIME}
  rm "${INPUT}.cr"
  echo "${multipart_middle_b2}" >>${OUT_MIME}
  echo >>${OUT_MIME}
  cat "$OUT_SIG" | ${BINDIR}/btoa | sed 's/\r$//' >>${OUT_MIME}
  echo "${multipart_end_b2}" >>${OUT_MIME}

  OUT_EML="${OUT_MIME}.eml"
  echo -n "${header_dave_mime_from_to_subject}" >>${OUT_EML}
  echo "clear-signed $SIG then clear-signed signed by dave" >>${OUT_EML}
  cat "${OUT_MIME}" >>${OUT_EML}
  echo >>${OUT_EML}
  sed -i"" "s/\$/$CR/" ${OUT_EML}

  INPUT="tb/alice.plain.${SIG}.opaque"
  OUT_SIG="${INPUT}.dave.d${SIG}"
  cat "$INPUT" | sed "s/\$/$CR/" > "${INPUT}.cr"
  ${PROFTOOL} ${BINDIR}/cmsutil -S -G -T -N Dave ${HASH_CMD} -i "${INPUT}.cr" -d ${P_R_DAVEDIR} -p nss -o "$OUT_SIG"

  OUT_MIME="${OUT_SIG}.multipart"
  echo "${multipart_start_b2}" | sed "s/HASHHASH/${HASH}/" >>${OUT_MIME}
  cat "${INPUT}.cr" | sed 's/\r$//' >>${OUT_MIME}
  rm "${INPUT}.cr"
  echo "${multipart_middle_b2}" >>${OUT_MIME}
  echo >>${OUT_MIME}
  cat "$OUT_SIG" | ${BINDIR}/btoa | sed 's/\r$//' >>${OUT_MIME}
  echo "${multipart_end_b2}" >>${OUT_MIME}

  OUT_EML="${OUT_MIME}.eml"
  echo -n "${header_dave_mime_from_to_subject}" >>${OUT_EML}
  echo "opaque-signed $SIG then clear-signed signed by dave" >>${OUT_EML}
  cat "${OUT_MIME}" >>${OUT_EML}
  echo >>${OUT_EML}
  sed -i"" "s/\$/$CR/" ${OUT_EML}
}

smime_enveloped_signed()
{
  SIG=sig.SHA${HASH}

  ${PROFTOOL} ${BINDIR}/cmsutil -S -G -T -N Alice ${HASH_CMD} -i tb/alice.env -d ${P_R_ALICEDIR} -p nss -o tb/alice.env.d${SIG}

  OUT="tb/alice.env.d${SIG}.multipart"
  echo "${multipart_start}" | sed "s/HASHHASH/${HASH}/" >>${OUT}
  cat tb/alice.env | sed 's/\r$//' >>${OUT}
  echo "${multipart_middle}" >>${OUT}
  cat tb/alice.env.d${SIG} | ${BINDIR}/btoa | sed 's/\r$//' >>${OUT}
  echo "${multipart_end}" >>${OUT}

  OUT="tb/alice.env.d${SIG}.multipart.eml"
  echo -n "${header_mime_from_to_subject}" >>${OUT}
  echo "enveloped then clear-signed ${SIG}" >>${OUT}
  cat "tb/alice.env.d${SIG}.multipart" >>${OUT}
  sed -i"" "s/\$/$CR/" ${OUT}

  ${PROFTOOL} ${BINDIR}/cmsutil -S -G -N Alice ${HASH_CMD} -i tb/alice.env -d ${P_R_ALICEDIR} -p nss -o tb/alice.env.${SIG}

  OUT="tb/alice.env.${SIG}.opaque"
  echo "$header_opaque_signed" >>${OUT}
  cat tb/alice.env.${SIG} | ${BINDIR}/btoa | sed 's/\r$//' >>${OUT}

  OUT="tb/alice.env.${SIG}.opaque.eml"
  echo -n "${header_mime_from_to_subject}" >>${OUT}
  echo "enveloped then opaque-signed $SIG" >>${OUT}
  cat "tb/alice.env.${SIG}.opaque" >>${OUT}
  echo >>${OUT}
  sed -i"" "s/\$/$CR/" ${OUT}

  # Second outer, opaque signature layer.

  INPUT="tb/alice.env.d${SIG}.multipart"
  OUT_SIG="${INPUT}.dave.${SIG}"
  ${PROFTOOL} ${BINDIR}/cmsutil -S -G -N Dave ${HASH_CMD} -i "$INPUT" -d ${P_R_DAVEDIR} -p nss -o "$OUT_SIG"

  OUT_MIME="${OUT_SIG}.opaque"
  echo "$header_opaque_signed" >>${OUT_MIME}
  cat "$OUT_SIG" | ${BINDIR}/btoa | sed 's/\r$//' >>${OUT_MIME}

  OUT_EML="${OUT_MIME}.eml"
  echo -n "${header_dave_mime_from_to_subject}" >>${OUT_EML}
  echo "enveloped then clear-signed $SIG then opaque signed by dave" >>${OUT_EML}
  cat "${OUT_MIME}" >>${OUT_EML}
  echo >>${OUT_EML}
  sed -i"" "s/\$/$CR/" ${OUT_EML}

  INPUT="tb/alice.env.${SIG}.opaque"
  OUT_SIG="${INPUT}.dave.${SIG}"
  ${PROFTOOL} ${BINDIR}/cmsutil -S -G -N Dave ${HASH_CMD} -i "$INPUT" -d ${P_R_DAVEDIR} -p nss -o "$OUT_SIG"

  OUT_MIME="${OUT_SIG}.opaque"
  echo "$header_opaque_signed" >>${OUT_MIME}
  cat "$OUT_SIG" | ${BINDIR}/btoa | sed 's/\r$//' >>${OUT_MIME}

  OUT_EML="${OUT_MIME}.eml"
  echo -n "${header_dave_mime_from_to_subject}" >>${OUT_EML}
  echo "enveloped then opaque-signed $SIG then opaque signed by dave" >>${OUT_EML}
  cat "${OUT_MIME}" >>${OUT_EML}
  echo >>${OUT_EML}
  sed -i"" "s/\$/$CR/" ${OUT_EML}
}

smime_p7()
{
  echo "$SCRIPTNAME: p7 util Data Tests ------------------------------"
  echo "p7env -d ${P_R_ALICEDIR} -r Alice -i alice.txt -o alice_p7.env"
  ${PROFTOOL} ${BINDIR}/p7env -d ${P_R_ALICEDIR} -r Alice -i alice.txt -o alice_p7.env
  html_msg $? 0 "Creating envelope for user Alice" "."

  echo "p7content -d ${P_R_ALICEDIR} -i alice_p7.env -o alice_p7.data"
  ${PROFTOOL} ${BINDIR}/p7content -d ${P_R_ALICEDIR} -i alice_p7.env -o alice_p7.data -p nss
  html_msg $? 0 "Verifying file delivered to user Alice" "."

  sed -e '3,3p' -n alice_p7.data > alice_p7.data.sed

  echo "diff alice.txt alice_p7.data.sed"
  diff alice.txt alice_p7.data.sed
  html_msg $? 0 "Compare Decoded Enveloped Data and Original" "."

  p7sig() {
    echo "p7sign -d ${P_R_ALICEDIR} -k Alice -i alice.txt -o alice.sig -p nss -e $alg $usage"
    ${PROFTOOL} ${BINDIR}/p7sign -d ${P_R_ALICEDIR} -k Alice -i alice.txt -o alice.sig -p nss -e $alg $usage
    html_msg $? $1 "Signing file for user Alice $alg $usage$2" "."
  }
  p7sigver() {
    p7sig 0 ''

    echo "p7verify -d ${P_R_ALICEDIR} -c alice.txt -s alice.sig $usage"
    ${PROFTOOL} ${BINDIR}/p7verify -d ${P_R_ALICEDIR} -c alice.txt -s alice.sig $usage
    html_msg $? 0 "Verifying file delivered to user Alice $alg $usage" "."
  }
  # no md2 or md5 (SEC_ERROR_SIGNATURE_ALGORITHM_DISABLED)
  for alg in "" "-a sha-1" "-a sha-256" "-a sha-384" "-a SHA-512" "-a SHA-224"; do
    usage=; p7sigver
    for usage in $(seq 0 12); do
      case $usage in
        2|3|6|10) usage="-u $usage"; p7sig 1 ' (inadequate)' ;; # SEC_ERROR_INADEQUATE_CERT_TYPE/SEC_ERROR_INADEQUATE_KEY_USAGE
        7|9)                                                 ;; # not well-liked by cert_VerifyCertWithFlags() on debug builds
        *)        usage="-u $usage"; p7sigver                ;;
      esac
    done
  done
}

############################## smime_main ##############################
# local shell function to test basic signed and enveloped messages
# from 1 --> 2"
########################################################################
smime_main()
{
  mime_init
  smime_enveloped

  HASH="1"
  cms_sign
  smime_signed_enveloped
  smime_plain_signed
  smime_enveloped_signed
  HASH="256"
  cms_sign
  smime_signed_enveloped
  smime_plain_signed
  smime_enveloped_signed
  HASH="384"
  cms_sign
  smime_signed_enveloped
  smime_plain_signed
  smime_enveloped_signed
  HASH="512"
  cms_sign
  smime_signed_enveloped
  smime_plain_signed
  smime_enveloped_signed

  echo "$SCRIPTNAME: Enveloped Data Tests ------------------------------"
  echo "cmsutil -E -r bob@example.com -i alice.txt -d ${P_R_ALICEDIR} -p nss \\"
  echo "        -o alice.env"
  ${PROFTOOL} ${BINDIR}/cmsutil -E -r bob@example.com -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice.env
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
  echo "        -r bob@example.com,dave@example.com"
  ${PROFTOOL} ${BINDIR}/cmsutil -E -i alice.txt -d ${P_R_ALICEDIR} -o alicecc.env \
          -r bob@example.com,dave@example.com
  ret=$?
  html_msg $ret 0 "Create Multiple Recipients Enveloped Data Alice" "."
  if [ $ret != 0 ] ; then
	echo "certutil -L -d ${P_R_ALICEDIR}"
	${BINDIR}/certutil -L -d ${P_R_ALICEDIR}
	echo "certutil -L -d ${P_R_ALICEDIR} -n dave@example.com"
	${BINDIR}/certutil -L -d ${P_R_ALICEDIR} -n dave@example.com
  fi

  echo "$SCRIPTNAME: Testing multiple email addrs ------------------------------"
  echo "cmsutil -E -i alice.txt -d ${P_R_ALICEDIR} -o aliceve.env \\"
  echo "        -r eve@example.net"
  ${PROFTOOL} ${BINDIR}/cmsutil -E -i alice.txt -d ${P_R_ALICEDIR} -o aliceve.env \
          -r eve@example.net
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
  echo "cmsutil -O -r \"Alice,bob@example.com,dave@example.com\" \\"
  echo "        -d ${P_R_ALICEDIR} > co.der"
  ${PROFTOOL} ${BINDIR}/cmsutil -O -r "Alice,bob@example.com,dave@example.com" -d ${P_R_ALICEDIR} > co.der
  html_msg $? 0 "Create Certs-Only Alice" "."

  echo "cmsutil -D -i co.der -d ${P_R_BOBDIR}"
  ${PROFTOOL} ${BINDIR}/cmsutil -D -i co.der -d ${P_R_BOBDIR}
  html_msg $? 0 "Verify Certs-Only by CA" "."

  echo "$SCRIPTNAME: Encrypted-Data Message ---------------------------------"
  echo "cmsutil -C -i alice.txt -e alicehello.env -d ${P_R_ALICEDIR} \\"
  echo "        -r \"bob@example.com\" > alice.enc"
  ${PROFTOOL} ${BINDIR}/cmsutil -C -i alice.txt -e alicehello.env -d ${P_R_ALICEDIR} \
          -r "bob@example.com" > alice.enc
  html_msg $? 0 "Create Encrypted-Data" "."

  echo "cmsutil -D -i alice.enc -d ${P_R_BOBDIR} -e alicehello.env -p nss \\"
  echo "        -o alice.data2"
  ${PROFTOOL} ${BINDIR}/cmsutil -D -i alice.enc -d ${P_R_BOBDIR} -e alicehello.env -p nss -o alice.data2
  html_msg $? 0 "Decode Encrypted-Data" "."

  diff alice.txt alice.data2
  html_msg $? 0 "Compare Decoded and Original Data" "."
}

smime_data_tb()
{
  ${BINDIR}/pk12util -d ${P_R_ALICEDIR} -o tb/Alice.p12 -n Alice -K nss -W nss
  ${BINDIR}/pk12util -d ${P_R_BOBDIR} -o tb/Bob.p12 -n Bob -K nss -W nss
  ${BINDIR}/pk12util -d ${P_R_DAVEDIR} -o tb/Dave.p12 -n Dave -K nss -W nss
  ${BINDIR}/pk12util -d ${P_R_EVEDIR} -o tb/Eve.p12 -n Eve -K nss -W nss
  CAOUT=tb/TestCA.pem
  cat ${P_R_CADIR}/TestCA.ca.cert | sed 's/\r$//' | ${BINDIR}/btoa -w c >> ${CAOUT}
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
smime_data_tb
smime_p7
smime_cleanup

