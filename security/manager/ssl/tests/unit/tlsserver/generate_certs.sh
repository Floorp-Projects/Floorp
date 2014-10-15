#!/bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Usage: ./generate_certs.sh <path to objdir> <output directory> [--clobber]
# e.g. (from the root of mozilla-central)
# `./security/manager/ssl/tests/unit/tlsserver/generate_certs.sh \
#  obj-x86_64-unknown-linux-gnu/ \
#  security/manager/ssl/tests/unit/tlsserver/`
#
# The --clobber switch is optional. If specified, the existing database of
# keys and certificates is removed and repopulated. By default, existing
# databases are preserved and only keys and certificates that don't already
# exist in the database are added.
# NB: If --clobber is specified, the following files to be overwritten if they
# are in the output directory:
#  cert9.db, key4.db, pkcs11.txt, test-ca.der, other-test-ca.der, default-ee.der
# (if --clobber is not specified, then only cert9.db and key4.db are modified)
# NB: If --clobber is specified, you must run genHPKPStaticPins.js after
# running this file, since its output (StaticHPKPins.h) depends on
# default-ee.der

set -x
set -e

if [ $# -lt 2 ]; then
  echo "Usage: `basename ${0}` <path to objdir> <output directory> [--clobber]"
  exit $E_BADARGS
fi

OBJDIR=${1}
OUTPUT_DIR=${2}
CLOBBER=0
if [ "${3}" == "--clobber" ]; then
  CLOBBER=1
fi
# Use the SQL DB so we can run tests on Android.
DB_ARGUMENT="sql:$OUTPUT_DIR"
RUN_MOZILLA="$OBJDIR/dist/bin/run-mozilla.sh"
CERTUTIL="$OBJDIR/dist/bin/certutil"
# On BSD, mktemp requires either a template or a prefix.
MKTEMP="mktemp temp.XXXX"

NOISE_FILE=`$MKTEMP`
# Make a good effort at putting something unique in the noise file.
date +%s%N  > "$NOISE_FILE"
PASSWORD_FILE=`$MKTEMP`

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

if [ -f "$OUTPUT_DIR/cert9.db" -o -f "$OUTPUT_DIR/key4.db" -o -f "$OUTPUT_DIR/pkcs11.txt" ]; then
  if [ $CLOBBER -eq 1 ]; then
    echo "Found pre-existing NSS DBs. Clobbering old certificates."
    rm -f "$OUTPUT_DIR/cert9.db" "$OUTPUT_DIR/key4.db" "$OUTPUT_DIR/pkcs11.txt"
    $RUN_MOZILLA $CERTUTIL -d $DB_ARGUMENT -N -f $PASSWORD_FILE
  else
    echo "Found pre-existing NSS DBs. Only generating newly added certificates."
    echo "(re-run with --clobber to remove and regenerate old certificates)"
  fi
else
  echo "No pre-existing NSS DBs found. Creating new ones."
  $RUN_MOZILLA $CERTUTIL -d $DB_ARGUMENT -N -f $PASSWORD_FILE
fi

COMMON_ARGS="-v 360 -w -1 -2 -z $NOISE_FILE"

function export_cert {
  NICKNAME="${1}"
  DERFILE="${2}"

  $RUN_MOZILLA $CERTUTIL -d $DB_ARGUMENT -L -n $NICKNAME -r > $OUTPUT_DIR/$DERFILE
}

# Bash doesn't actually allow return values in a sane way, so just use a
# global variable.
function cert_already_exists {
  NICKNAME="${1}"
  ALREADY_EXISTS=1
  $RUN_MOZILLA $CERTUTIL -d $DB_ARGUMENT -L -n $NICKNAME || ALREADY_EXISTS=0
}

function make_CA {
  CA_RESPONSES="y\n1\ny"
  NICKNAME="${1}"
  SUBJECT="${2}"
  DERFILE="${3}"

  cert_already_exists $NICKNAME
  if [ $ALREADY_EXISTS -eq 1 ]; then
    echo "cert \"$NICKNAME\" already exists - not regenerating it (use --clobber to force regeneration)"
    return
  fi

  echo -e "$CA_RESPONSES" | $RUN_MOZILLA $CERTUTIL -d $DB_ARGUMENT -S \
                                                   -n $NICKNAME \
                                                   -s "$SUBJECT" \
                                                   -t "CT,," \
                                                   -x $COMMON_ARGS
  export_cert $NICKNAME $DERFILE
}

SERIALNO=$RANDOM

function make_INT {
  INT_RESPONSES="y\n0\ny\n2\n7\nhttp://localhost:8080/\n\nn\nn\n"
  NICKNAME="${1}"
  SUBJECT="${2}"
  CA="${3}"
  EXTRA_ARGS="${4}"

  cert_already_exists $NICKNAME
  if [ $ALREADY_EXISTS -eq 1 ]; then
    echo "cert \"$NICKNAME\" already exists - not regenerating it (use --clobber to force regeneration)"
    return
  fi

  echo -e "$INT_RESPONSES" | $RUN_MOZILLA $CERTUTIL -d $DB_ARGUMENT -S \
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

# This creates an X.509 version 1 certificate (note --certVersion 1 and a lack
# of extensions).
function make_V1 {
  NICKNAME="${1}"
  SUBJECT="${2}"
  CA="${3}"

  cert_already_exists $NICKNAME
  if [ $ALREADY_EXISTS -eq 1 ]; then
    echo "cert \"$NICKNAME\" already exists - not regenerating it (use --clobber to force regeneration)"
    return
  fi

  $RUN_MOZILLA $CERTUTIL -d $DB_ARGUMENT -S \
                         -n $NICKNAME \
                         -s "$SUBJECT" \
                         -c $CA \
                         -t ",," \
                         -m $SERIALNO \
                         --certVersion 1 \
                         -v 360 -w -1 -z $NOISE_FILE

  SERIALNO=$(($SERIALNO + 1))
}

function make_EE {
  CERT_RESPONSES="n\n\ny\n2\n7\nhttp://localhost:8080/\n\nn\nn\n"
  NICKNAME="${1}"
  SUBJECT="${2}"
  CA="${3}"
  SUBJECT_ALT_NAME="${4}"
  EXTRA_ARGS="${5} ${6}"

  cert_already_exists $NICKNAME
  if [ $ALREADY_EXISTS -eq 1 ]; then
    echo "cert \"$NICKNAME\" already exists - not regenerating it (use --clobber to force regeneration)"
    return
  fi

  echo -e "$CERT_RESPONSES" | $RUN_MOZILLA $CERTUTIL -d $DB_ARGUMENT -S \
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

function make_EE_with_nsCertType {
  NICKNAME="${1}"
  SUBJECT="${2}"
  CA="${3}"
  SUBJECT_ALT_NAME="${4}"
  NS_CERT_TYPE_CRITICAL="${5}"
  EXTRA_ARGS="${6}"
  # This adds the Netscape certificate type extension with the "sslServer"
  # bit asserted. Its criticality depends on if "y" or "n" was passed as
  # an argument to this function.
  CERT_RESPONSES="n\n\ny\n1\n8\n$NS_CERT_TYPE_CRITICAL\n"

  cert_already_exists $NICKNAME
  if [ $ALREADY_EXISTS -eq 1 ]; then
    echo "cert \"$NICKNAME\" already exists - not regenerating it (use --clobber to force regeneration)"
    return
  fi

  echo -e "$CERT_RESPONSES" | $RUN_MOZILLA $CERTUTIL -d $DB_ARGUMENT -S \
                                                     -n $NICKNAME \
                                                     -s "$SUBJECT" \
                                                     -8 $SUBJECT_ALT_NAME \
                                                     -c $CA \
                                                     -t ",," \
                                                     -m $SERIALNO \
                                                     -5 \
                                                     $COMMON_ARGS \
                                                     $EXTRA_ARGS
  SERIALNO=$(($SERIALNO + 1))
}

function make_delegated {
  CERT_RESPONSES="n\n\ny\n"
  NICKNAME="${1}"
  SUBJECT="${2}"
  CA="${3}"
  EXTRA_ARGS="${4}"

  cert_already_exists $NICKNAME
  if [ $ALREADY_EXISTS -eq 1 ]; then
    echo "cert \"$NICKNAME\" already exists - not regenerating it (use --clobber to force regeneration)"
    return
  fi

  echo -e "$CERT_RESPONSES" | $RUN_MOZILLA $CERTUTIL -d $DB_ARGUMENT -S \
                                                     -n $NICKNAME \
                                                     -s "$SUBJECT" \
                                                     -c $CA \
                                                     -t ",," \
                                                     -m $SERIALNO \
                                                     $COMMON_ARGS \
                                                     $EXTRA_ARGS
  SERIALNO=$(($SERIALNO + 1))
}

make_CA testCA 'CN=Test CA' test-ca.der
make_CA otherCA 'CN=Other test CA' other-test-ca.der

make_EE localhostAndExampleCom 'CN=Test End-entity' testCA "localhost,*.example.com,*.pinning.example.com,*.include-subdomains.pinning.example.com,*.exclude-subdomains.pinning.example.com"
# Make an EE cert issued by otherCA
make_EE otherIssuerEE 'CN=Wrong CA Pin Test End-Entity' otherCA "*.include-subdomains.pinning.example.com,*.exclude-subdomains.pinning.example.com,*.pinning.example.com"

export_cert localhostAndExampleCom default-ee.der

# A cert that is like localhostAndExampleCom, but with a different serial number for
# testing the "OCSP response is from the right issuer, but it is for the wrong cert"
# case.
make_EE ocspOtherEndEntity 'CN=Other Cert' testCA "localhost,*.example.com"

make_INT testINT 'CN=Test Intermediate' testCA
make_EE ocspEEWithIntermediate 'CN=Test End-entity with Intermediate' testINT "localhost,*.example.com"
make_EE expired 'CN=Expired Test End-entity' testCA "expired.example.com" "-w -400"
export_cert expired expired-ee.der
make_EE mismatch 'CN=Mismatch Test End-entity' testCA "doesntmatch.example.com"
make_EE selfsigned 'CN=Self-signed Test End-entity' testCA "selfsigned.example.com" "-x"
# If the certificate 'CN=Test Intermediate' isn't loaded into memory,
# this certificate will have an unknown issuer.
# deletedINT is never kept in the database, so it always gets regenerated.
# That's ok, because if unknownissuer was already in the database, it won't
# get regenerated. Either way, deletedINT will then be removed again.
make_INT deletedINT 'CN=Test Intermediate to delete' testCA
make_EE unknownissuer 'CN=Test End-entity from unknown issuer' deletedINT "unknownissuer.example.com"

$RUN_MOZILLA $CERTUTIL -d $DB_ARGUMENT -D -n deletedINT

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
export_cert inadequatekeyusage inadequatekeyusage-ee.der
make_EE selfsigned-inadequateEKU 'CN=Self-signed Inadequate EKU Test End-entity' unused "selfsigned-inadequateEKU.example.com" "--keyUsage keyEncipherment,dataEncipherment --extKeyUsage serverAuth" "-x"

make_delegated delegatedSigner 'CN=Test Delegated Responder' testCA "--extKeyUsage ocspResponder"
make_delegated invalidDelegatedSignerNoExtKeyUsage 'CN=Test Invalid Delegated Responder No extKeyUsage' testCA
make_delegated invalidDelegatedSignerFromIntermediate 'CN=Test Invalid Delegated Responder From Intermediate' testINT "--extKeyUsage ocspResponder"
make_delegated invalidDelegatedSignerKeyUsageCrlSigning 'CN=Test Invalid Delegated Responder keyUsage crlSigning' testCA "--keyUsage crlSigning"
make_delegated invalidDelegatedSignerWrongExtKeyUsage 'CN=Test Invalid Delegated Responder Wrong extKeyUsage' testCA "--extKeyUsage codeSigning"

make_INT self-signed-EE-with-cA-true 'CN=Test Self-signed End-entity with CA true' unused "-x -8 self-signed-end-entity-with-cA-true.example.com"
make_INT ca-used-as-end-entity 'CN=Test Intermediate used as End-Entity' testCA "-8 ca-used-as-end-entity.example.com"

make_delegated badKeysizeDelegatedSigner 'CN=Bad Keysize Delegated Responder' testCA "--extKeyUsage ocspResponder -g 1008"

make_EE_with_nsCertType nsCertTypeCritical 'CN=nsCertType Critical' testCA "localhost,*.example.com" "y"
make_EE_with_nsCertType nsCertTypeNotCritical 'CN=nsCertType Not Critical' testCA "localhost,*.example.com" "n"
make_EE_with_nsCertType nsCertTypeCriticalWithExtKeyUsage 'CN=nsCertType Critical With extKeyUsage' testCA "localhost,*.example.com" "y" "--extKeyUsage serverAuth"

# Make an X.509 version 1 certificate that will issue another certificate.
# By default, this causes an error in verification that we allow overrides for.
# However, if the v1 certificate is a trust anchor, then verification succeeds.
make_V1 v1Cert 'CN=V1 Cert' testCA
export_cert v1Cert v1Cert.der
make_EE eeIssuedByV1Cert 'CN=EE Issued by V1 Cert' v1Cert "localhost,*.example.com"

cleanup
