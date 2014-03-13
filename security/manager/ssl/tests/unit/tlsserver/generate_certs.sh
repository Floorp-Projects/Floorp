#!/bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Usage: ./generate_certs.sh <path to objdir> <output directory>
# e.g. (from the root of mozilla-central)
# `./security/manager/ssl/tests/unit/tlsserver/generate_certs.sh \
#  obj-x86_64-unknown-linux-gnu/ \
#  security/manager/ssl/tests/unit/tlsserver/`
#
# NB: This will cause the following files to be overwritten if they are in
# the output directory:
#  cert8.db, key3.db, secmod.db, ocsp-ca.der, ocsp-other-ca.der
set -x
set -e

if [ $# -ne 2 ]; then
  echo "Usage: `basename ${0}` <path to objdir> <output directory>"
  exit $E_BADARGS
fi

OBJDIR=${1}
OUTPUT_DIR=${2}
RUN_MOZILLA="$OBJDIR/dist/bin/run-mozilla.sh"
CERTUTIL="$OBJDIR/dist/bin/certutil"

NOISE_FILE=`mktemp`
# Make a good effort at putting something unique in the noise file.
date +%s%N  > "$NOISE_FILE"
PASSWORD_FILE=`mktemp`

function cleanup {
  rm -f "$NOISE_FILE" "$PASSWORD_FILE"
}

if [ ! -f "$RUN_MOZILLA" ]; then
  echo "Could not find run-mozilla.sh at \'$RUN_MOZILLA\' - I'll try without it"
  RUN_MOZILLA=""
fi

if [ ! -f "$CERTUTIL" ]; then
  echo "Could not find certutil at \'$CERTUTIL\'"
  exit $E_BADARGS
fi

if [ ! -d "$OUTPUT_DIR" ]; then
  echo "Could not find output directory at \'$OUTPUT_DIR\'"
  exit $E_BADARGS
fi

if [ -f "$OUTPUT_DIR/cert8.db" -o -f "$OUTPUT_DIR/key3.db" -o -f "$OUTPUT_DIR/secmod.db" ]; then
  echo "Found pre-existing NSS DBs. Clobbering old OCSP certs."
  rm -f "$OUTPUT_DIR/cert8.db" "$OUTPUT_DIR/key3.db" "$OUTPUT_DIR/secmod.db"
fi
$RUN_MOZILLA $CERTUTIL -d $OUTPUT_DIR -N -f $PASSWORD_FILE

COMMON_ARGS="-v 360 -w -1 -2 -z $NOISE_FILE"

function make_CA {
  CA_RESPONSES="y\n1\ny"
  NICKNAME="${1}"
  SUBJECT="${2}"
  DERFILE="${3}"

  echo -e "$CA_RESPONSES" | $RUN_MOZILLA $CERTUTIL -d $OUTPUT_DIR -S \
                                                   -n $NICKNAME \
                                                   -s "$SUBJECT" \
                                                   -t "CT,," \
                                                   -x $COMMON_ARGS
  $RUN_MOZILLA $CERTUTIL -d $OUTPUT_DIR -L -n $NICKNAME -r > $OUTPUT_DIR/$DERFILE
}

SERIALNO=1

function make_INT {
  INT_RESPONSES="y\n0\ny\n2\n7\nhttp://localhost:8080/\n\nn\nn\n"
  NICKNAME="${1}"
  SUBJECT="${2}"
  CA="${3}"
  EXTRA_ARGS="${4}"

  echo -e "$INT_RESPONSES" | $RUN_MOZILLA $CERTUTIL -d $OUTPUT_DIR -S \
                                                    -n $NICKNAME \
                                                    -s "$SUBJECT" \
                                                    -c $CA \
                                                    -t ",," \
                                                    -m $SERIALNO \
                                                    --extAIA \
                                                    $COMMON_ARGS \
                                                    $EXTRA_ARGS
  SERIALNO=$(($SERIALNO + 1))
}

function make_EE {
  CERT_RESPONSES="n\n\ny\n2\n7\nhttp://localhost:8080/\n\nn\nn\n"
  NICKNAME="${1}"
  SUBJECT="${2}"
  CA="${3}"
  SUBJECT_ALT_NAME="${4}"
  EXTRA_ARGS="${5} ${6}"

  echo -e "$CERT_RESPONSES" | $RUN_MOZILLA $CERTUTIL -d $OUTPUT_DIR -S \
                                                     -n $NICKNAME \
                                                     -s "$SUBJECT" \
                                                     -8 $SUBJECT_ALT_NAME \
                                                     -c $CA \
                                                     -t ",," \
                                                     -m $SERIALNO \
                                                     --extAIA \
                                                     $COMMON_ARGS \
                                                     $EXTRA_ARGS
  SERIALNO=$(($SERIALNO + 1))
}

make_CA testCA 'CN=Test CA' test-ca.der
make_CA otherCA 'CN=Other test CA' other-test-ca.der
make_EE localhostAndExampleCom 'CN=Test End-entity' testCA "localhost,*.example.com"
$RUN_MOZILLA $CERTUTIL -d $OUTPUT_DIR -L -n localhostAndExampleCom -r > $OUTPUT_DIR/default-ee.der
# A cert that is like localhostAndExampleCom, but with a different serial number for
# testing the "OCSP response is from the right issuer, but it is for the wrong cert"
# case.
make_EE ocspOtherEndEntity 'CN=Other Cert' testCA "localhost,*.example.com"

make_INT testINT 'CN=Test Intermediate' testCA
make_EE ocspEEWithIntermediate 'CN=Test End-entity with Intermediate' testINT "localhost,*.example.com"
make_EE expired 'CN=Expired Test End-entity' testCA "expired.example.com" "-w -400"
make_EE mismatch 'CN=Mismatch Test End-entity' testCA "doesntmatch.example.com"
make_EE selfsigned 'CN=Self-signed Test End-entity' testCA "selfsigned.example.com" "-x"
# If the certificate 'CN=Test Intermediate' isn't loaded into memory,
# this certificate will have an unknown issuer.
make_INT deletedINT 'CN=Test Intermediate to delete' testCA
make_EE unknownissuer 'CN=Test End-entity from unknown issuer' deletedINT "unknownissuer.example.com"
$RUN_MOZILLA $CERTUTIL -d $OUTPUT_DIR -D -n deletedINT
make_INT expiredINT 'CN=Expired Test Intermediate' testCA "-w -400"
make_EE expiredissuer 'CN=Test End-entity with expired issuer' expiredINT "expiredissuer.example.com"
NSS_ALLOW_WEAK_SIGNATURE_ALG=1 make_EE md5signature 'CN=Test End-entity with MD5 signature' testCA "md5signature.example.com" "-Z MD5"
make_EE untrustedissuer 'CN=Test End-entity with untrusted issuer' otherCA "untrustedissuer.example.com"

make_EE mismatch-expired 'CN=Mismatch-Expired Test End-entity' testCA "doesntmatch.example.com" "-w -400"
make_EE mismatch-untrusted 'CN=Mismatch-Untrusted Test End-entity' otherCA "doesntmatch.example.com"
make_EE untrusted-expired 'CN=Untrusted-Expired Test End-entity' otherCA "untrusted-expired.example.com" "-w -400"
make_EE mismatch-untrusted-expired 'CN=Mismatch-Untrusted-Expired Test End-entity' otherCA "doesntmatch.example.com" "-w -400"
NSS_ALLOW_WEAK_SIGNATURE_ALG=1 make_EE md5signature-expired 'CN=Test MD5Signature-Expired End-entity' testCA "md5signature-expired.example.com" "-Z MD5" "-w -400"

make_EE inadequatekeyusage 'CN=Inadequate Key Usage Test End-entity' testCA "inadequatekeyusage.example.com" "--keyUsage crlSigning"
make_EE selfsigned-inadequateEKU 'CN=Self-signed Inadequate EKU Test End-entity' unused "selfsigned-inadequateEKU.example.com" "--keyUsage keyEncipherment,dataEncipherment --extKeyUsage serverAuth" "-x"

cleanup
