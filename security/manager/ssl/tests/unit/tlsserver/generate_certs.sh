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
  CA_RESPONSES="y\n0\ny"
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

function make_cert {
  CERT_RESPONSES="n\n\ny"
  NICKNAME="${1}"
  SUBJECT="${2}"
  CA="${3}"
  SUBJECT_ALT_NAME="${4}"
  if [ -n "$SUBJECT_ALT_NAME" ]; then
    SUBJECT_ALT_NAME="-8 $SUBJECT_ALT_NAME"
  fi

  echo -e "$CERT_RESPONSES" | $RUN_MOZILLA $CERTUTIL -d $OUTPUT_DIR -S \
                                                     -n $NICKNAME \
                                                     -s "$SUBJECT" \
                                                     $SUBJECT_ALT_NAME \
                                                     -c $CA \
                                                     -t ",," \
                                                     -m $SERIALNO \
                                                     $COMMON_ARGS
  SERIALNO=$(($SERIALNO + 1))
}

make_CA testCA 'CN=Test CA' test-ca.der
make_CA otherCA 'CN=Other test CA' other-test-ca.der
make_cert localhostAndExampleCom 'CN=Test End-entity' testCA "localhost,*.example.com"
# A cert that is like localhostAndExampleCom, but with a different serial number for
# testing the "OCSP response is from the right issuer, but it is for the wrong cert"
# case.
make_cert ocspOtherEndEntity 'CN=Other Cert' testCA "localhost,*.example.com"

cleanup
