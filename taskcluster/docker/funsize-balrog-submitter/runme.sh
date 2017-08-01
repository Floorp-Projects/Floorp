#!/bin/bash

set -xe

test $PARENT_TASK_ARTIFACTS_URL_PREFIX
test $BALROG_API_ROOT
test $SHA1_SIGNING_CERT
test $SHA384_SIGNING_CERT


ARTIFACTS_DIR="/home/worker/artifacts"
mkdir -p "$ARTIFACTS_DIR"

curl --location --retry 10 --retry-delay 10 -o "$ARTIFACTS_DIR/manifest.json" \
    "$PARENT_TASK_ARTIFACTS_URL_PREFIX/manifest.json"

cat "$ARTIFACTS_DIR/manifest.json"
python /home/worker/bin/funsize-balrog-submitter.py \
    --artifacts-url-prefix "$PARENT_TASK_ARTIFACTS_URL_PREFIX" \
    --manifest "$ARTIFACTS_DIR/manifest.json" \
    -a "$BALROG_API_ROOT" \
    --sha1-signing-cert "/home/worker/keys/${SHA1_SIGNING_CERT}.pubkey" \
    --sha384-signing-cert "/home/worker/keys/${SHA384_SIGNING_CERT}.pubkey" \
    --verbose \
    $EXTRA_BALROG_SUBMITTER_PARAMS
