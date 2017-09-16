#!/bin/sh

set -xe

test $TASK_ID
test $SHA1_SIGNING_CERT
test $SHA384_SIGNING_CERT

ARTIFACTS_DIR="/home/worker/artifacts"
mkdir -p "$ARTIFACTS_DIR"

curl --location --retry 10 --retry-delay 10 -o /home/worker/task.json \
    "https://queue.taskcluster.net/v1/task/$TASK_ID"

if [ ! -z $OUTPUT_FILENAME ]; then
    EXTRA_PARAMS="--output-filename $OUTPUT_FILENAME $EXTRA_PARAMS"
fi

/home/worker/bin/recompress.py \
    --artifacts-dir "$ARTIFACTS_DIR" \
    --task-definition /home/worker/task.json \
    --sha1-signing-cert "/home/worker/keys/${SHA1_SIGNING_CERT}.pubkey" \
    --sha384-signing-cert "/home/worker/keys/${SHA384_SIGNING_CERT}.pubkey" \
    $EXTRA_PARAMS
