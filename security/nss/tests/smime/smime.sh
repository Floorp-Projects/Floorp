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
  html_head "S/MIME Tests"

  grep "SUCCESS: SMIME passed" $CERT_LOG_FILE >/dev/null || {
      Exit 11 "Fatal - S/MIME of cert.sh needs to pass first"
  }

  SMIMEDIR=${HOSTDIR}/smime
  R_SMIMEDIR=../smime
  mkdir -p ${SMIMEDIR}
  cd ${SMIMEDIR}
  cp ${QADIR}/smime/alice.txt ${SMIMEDIR}
}


############################## smime_main ##############################
# local shell function to test basic signed and enveloped messages 
# from 1 --> 2"
########################################################################
smime_main()
{

  echo "$SCRIPTNAME: Signing Attached Message (SHA1) ------------------"
  echo "cmsutil -S -N Alice -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice.sig"
  cmsutil -S -N Alice -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice.sig
  html_msg $? 0 "Create Signature Alice (SHA1)" "."

  echo "cmsutil -D -i alice.sig -d ${P_R_BOBDIR} -o alice.data1"
  cmsutil -D -i alice.sig -d ${P_R_BOBDIR} -o alice.data1
  html_msg $? 0 "Decode Alice's Signature (SHA1)" "."

  echo "diff alice.txt alice.data1"
  diff alice.txt alice.data1
  html_msg $? 0 "Compare Decoded Signature and Original (SHA1)" "."

  echo "$SCRIPTNAME: Signing Attached Message (SHA256) ------------------"
  echo "cmsutil -S -N Alice -H SHA256 -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice.sig"
  cmsutil -S -N Alice -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice.sig
  html_msg $? 0 "Create Signature Alice (SHA256)" "."

  echo "cmsutil -D -i alice.sig -d ${P_R_BOBDIR} -o alice.data1"
  cmsutil -D -i alice.sig -d ${P_R_BOBDIR} -o alice.data1
  html_msg $? 0 "Decode Alice's Signature (SHA256)" "."

  echo "diff alice.txt alice.data1"
  diff alice.txt alice.data1
  html_msg $? 0 "Compare Decoded Signature and Original (SHA256)" "."

  echo "$SCRIPTNAME: Signing Attached Message (SHA384) ------------------"
  echo "cmsutil -S -N Alice -H SHA384 -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice.sig"
  cmsutil -S -N Alice -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice.sig
  html_msg $? 0 "Create Signature Alice (SHA384)" "."

  echo "cmsutil -D -i alice.sig -d ${P_R_BOBDIR} -o alice.data1"
  cmsutil -D -i alice.sig -d ${P_R_BOBDIR} -o alice.data1
  html_msg $? 0 "Decode Alice's Signature (SHA384)" "."

  echo "diff alice.txt alice.data1"
  diff alice.txt alice.data1
  html_msg $? 0 "Compare Decoded Signature and Original (SHA384)" "."

  echo "$SCRIPTNAME: Signing Attached Message (SHA512) ------------------"
  echo "cmsutil -S -N Alice -H SHA512 -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice.sig"
  cmsutil -S -N Alice -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice.sig
  html_msg $? 0 "Create Signature Alice (SHA512)" "."

  echo "cmsutil -D -i alice.sig -d ${P_R_BOBDIR} -o alice.data1"
  cmsutil -D -i alice.sig -d ${P_R_BOBDIR} -o alice.data1
  html_msg $? 0 "Decode Alice's Signature (SHA512)" "."

  echo "diff alice.txt alice.data1"
  diff alice.txt alice.data1
  html_msg $? 0 "Compare Decoded Signature and Original (SHA512)" "."

  echo "$SCRIPTNAME: Enveloped Data Tests ------------------------------"
  echo "cmsutil -E -r bob@bogus.com -i alice.txt -d ${P_R_ALICEDIR} -p nss \\"
  echo "        -o alice.env"
  cmsutil -E -r bob@bogus.com -i alice.txt -d ${P_R_ALICEDIR} -p nss -o alice.env
  html_msg $? 0 "Create Enveloped Data Alice" "."

  echo "cmsutil -D -i alice.env -d ${P_R_BOBDIR} -p nss -o alice.data1"
  cmsutil -D -i alice.env -d ${P_R_BOBDIR} -p nss -o alice.data1
  html_msg $? 0 "Decode Enveloped Data Alice" "."

  echo "diff alice.txt alice.data1"
  diff alice.txt alice.data1
  html_msg $? 0 "Compare Decoded Enveloped Data and Original" "."

  # multiple recip
  echo "$SCRIPTNAME: Testing multiple recipients ------------------------------"
  echo "cmsutil -E -i alicecc.txt -d ${P_R_ALICEDIR} -o alicecc.env \\"
  echo "        -r bob@bogus.com,dave@bogus.com"
  cmsutil -E -i alice.txt -d ${P_R_ALICEDIR} -o alicecc.env \
          -r bob@bogus.com,dave@bogus.com
  ret=$?
  html_msg $ret 0 "Create Multiple Recipients Enveloped Data Alice" "."
  if [ $ret != 0 ] ; then
        i=0
        echo "cp -r ${R_ALICEDIR} ${R_ALICEDIR}.trouble"
        cp -r ${R_ALICEDIR} ${R_ALICEDIR}.trouble
        while [ $i -lt 100 ] ; do
            echo "will attempt to list the certs in the db `expr 100 - $i` more times"
	    echo "certutil -L -d ${P_R_ALICEDIR}"
	    certutil -L -d ${P_R_ALICEDIR}
	    echo "certutil -L -d ${P_R_ALICEDIR} -n dave@bogus.com"
	    certutil -L -d ${P_R_ALICEDIR} -n dave@bogus.com
            sleep 30
            i=`expr $i + 1`
        done
  fi

  echo "$SCRIPTNAME: Testing multiple email addrs ------------------------------"
  echo "cmsutil -E -i alicecc.txt -d ${P_R_ALICEDIR} -o aliceve.env \\"
  echo "        -r eve@bogus.net"
  cmsutil -E -i alice.txt -d ${P_R_ALICEDIR} -o aliceve.env \
          -r eve@bogus.net
  ret=$?
  html_msg $ret 0 "Encrypt to a Multiple Email cert" "."

  echo "cmsutil -D -i alicecc.env -d ${P_R_BOBDIR} -p nss -o alice.data2"
  cmsutil -D -i alicecc.env -d ${P_R_BOBDIR} -p nss -o alice.data2
  html_msg $? 0 "Decode Multiple Recipients Enveloped Data Alice by Bob" "."

  echo "cmsutil -D -i alicecc.env -d ${P_R_DAVEDIR} -p nss -o alice.data3"
  cmsutil -D -i alicecc.env -d ${P_R_DAVEDIR} -p nss -o alice.data3
  html_msg $? 0 "Decode Multiple Recipients Enveloped Data Alice by Dave" "."

  echo "cmsutil -D -i aliceve.env -d ${P_R_EVEDIR} -p nss -o alice.data4"
  cmsutil -D -i aliceve.env -d ${P_R_EVEDIR} -p nss -o alice.data4
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
  cmsutil -O -r "Alice,bob@bogus.com,dave@bogus.com" -d ${P_R_ALICEDIR} > co.der
  html_msg $? 0 "Create Certs-Only Alice" "."

  echo "cmsutil -D -i co.der -d ${P_R_BOBDIR}"
  cmsutil -D -i co.der -d ${P_R_BOBDIR}
  html_msg $? 0 "Verify Certs-Only by CA" "."

  echo "$SCRIPTNAME: Encrypted-Data Message ---------------------------------"
  echo "cmsutil -C -i alice.txt -e alicehello.env -d ${P_R_ALICEDIR} \\"
  echo "        -r \"bob@bogus.com\" > alice.enc"
  cmsutil -C -i alice.txt -e alicehello.env -d ${P_R_ALICEDIR} \
          -r "bob@bogus.com" > alice.enc
  html_msg $? 0 "Create Encrypted-Data" "."

  echo "cmsutil -D -i alice.enc -d ${P_R_BOBDIR} -e alicehello.env -p nss \\"
  echo "        -o alice.data2"
  cmsutil -D -i alice.enc -d ${P_R_BOBDIR} -e alicehello.env -p nss -o alice.data2
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
smime_cleanup

