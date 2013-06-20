#!/bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Usage: ./gen_ocsp_certs.sh <path to objdir> <output directory>
# e.g. (from the root of mozilla-central)
# `./security/manager/ssl/tests/unit/test_ocsp_stapling/gen_ocsp_certs.sh \
#  obj-x86_64-unknown-linux-gnu/ \
#  security/manager/ssl/tests/unit/test_ocsp_stapling/`
#
# NB: This will cause the following files to be overwritten if they are in
# the output directory:
#  cert8.db, key3.db, secmod.db, ocsp-ca.der, ocsp-other-ca.der

if [ $# -ne 2 ]; then
  echo "Usage: `basename ${0}` <path to objdir> <output directory>"
  exit $E_BADARGS
fi

OBJDIR=${1}
OUTPUT_DIR=${2}
RUN_MOZILLA="$OBJDIR/dist/bin/run-mozilla.sh"
CERTUTIL="$OBJDIR/dist/bin/certutil"

function check_retval {
  retval=$?
  if [ "$retval" -ne 0 ]; then
    echo "failed..."
    exit "$retval"
  fi
}

NOISE_FILE=`mktemp`
echo "running \"dd if=/dev/urandom of="$NOISE_FILE" bs=1024 count=8\""
dd if=/dev/urandom of="$NOISE_FILE" bs=1024 count=1
check_retval
PASSWORD_FILE=`mktemp`

function cleanup {
  rm -f "$NOISE_FILE" "$PASSWORD_FILE"
}

if [ ! -f "$RUN_MOZILLA" ]; then
  echo "Could not find run-mozilla.sh at \'$RUN_MOZILLA\'"
  exit $E_BADARGS
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
echo "running \"$RUN_MOZILLA $CERTUTIL -d $OUTPUT_DIR -N -f $PASSWORD_FILE\""
$RUN_MOZILLA $CERTUTIL -d $OUTPUT_DIR -N -f $PASSWORD_FILE
check_retval

COMMON_ARGS="-v 360 -w -1 -2 -z $NOISE_FILE"

function make_CA {
  CA_RESPONSES="y\n0\ny"
  NICKNAME="${1}"
  SUBJECT="${2}"
  DERFILE="${3}"

  check_retval
  echo "running 'echo -e \"$CA_RESPONSES\" | $RUN_MOZILLA $CERTUTIL -d $OUTPUT_DIR -S -n $NICKNAME -s \"$SUBJECT\" -t CTu,u,u -x $COMMON_ARGS'"
  echo -e "$CA_RESPONSES" | $RUN_MOZILLA $CERTUTIL -d $OUTPUT_DIR -S -n $NICKNAME -s "$SUBJECT" -t CTu,u,u -x $COMMON_ARGS
  check_retval
  echo "running \"$RUN_MOZILLA $CERTUTIL -d $OUTPUT_DIR -L -n $NICKNAME -r > $OUTPUT_DIR/$DERFILE\""
  $RUN_MOZILLA $CERTUTIL -d $OUTPUT_DIR -L -n $NICKNAME -r > $OUTPUT_DIR/$DERFILE
  check_retval
}

SERIALNO=1

function make_cert {
  CERT_RESPONSES="n\n\ny"
  NICKNAME="${1}"
  SUBJECT="${2}"
  CA="${3}"

  echo "running 'echo -e \"$CERT_RESPONSES\" | $RUN_MOZILLA $CERTUTIL -d $OUTPUT_DIR -S -s \"$SUBJECT\" -n $NICKNAME -c $CA -t Pu,u,u -m $SERIALNO $COMMON_ARGS'"
  echo -e "$CERT_RESPONSES" | $RUN_MOZILLA $CERTUTIL -d $OUTPUT_DIR -S -s "$SUBJECT" -n $NICKNAME -c $CA -t Pu,u,u -m $SERIALNO $COMMON_ARGS
  check_retval
  SERIALNO=$(($SERIALNO + 1))
}

make_CA ocspTestCA 'CN=OCSP stapling test CA' ocsp-ca.der
make_CA otherCA 'CN=OCSP other test CA' ocsp-other-ca.der

make_cert localhost 'CN=localhost' ocspTestCA
make_cert good 'CN=ocsp-stapling-good.example.com' ocspTestCA
make_cert revoked 'CN=ocsp-stapling-revoked.example.com' ocspTestCA
make_cert unknown 'CN=ocsp-stapling-unknown.example.com' ocspTestCA
make_cert good-other 'CN=ocsp-stapling-good-other.example.com' ocspTestCA
make_cert good-otherCA 'CN=ocsp-stapling-good-other-ca.example.com' ocspTestCA
make_cert expired 'CN=ocsp-stapling-expired.example.com' ocspTestCA
make_cert expired-freshCA 'CN=ocsp-stapling-expired-fresh-ca.example.com' ocspTestCA
make_cert none 'CN=ocsp-stapling-none.example.com' ocspTestCA
make_cert malformed 'CN=ocsp-stapling-malformed.example.com' ocspTestCA
make_cert srverr 'CN=ocsp-stapling-srverr.example.com' ocspTestCA
make_cert trylater 'CN=ocsp-stapling-trylater.example.com' ocspTestCA
make_cert needssig 'CN=ocsp-stapling-needssig.example.com' ocspTestCA
make_cert unauthorized 'CN=ocsp-stapling-unauthorized.example.com' ocspTestCA

cleanup
