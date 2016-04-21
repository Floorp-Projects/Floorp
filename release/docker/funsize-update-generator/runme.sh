#!/bin/sh

set -xe

test $TASK_ID
test $SIGNING_CERT

ARTIFACTS_DIR="/home/worker/artifacts"
mkdir -p "$ARTIFACTS_DIR"

curl --location --retry 10 --retry-delay 10 -o /home/worker/task.json \
    "https://queue.taskcluster.net/v1/task/$TASK_ID"

# enable locale cache
export MBSDIFF_HOOK="/home/worker/bin/mbsdiff_hook.sh -c /tmp/fs-cache"

if [ ! -z $FILENAME_TEMPLATE ]; then
    EXTRA_PARAMS="--filename-template $FILENAME_TEMPLATE $EXTRA_PARAMS"
fi

/home/worker/bin/funsize.py \
    --artifacts-dir "$ARTIFACTS_DIR" \
    --task-definition /home/worker/task.json \
    --signing-cert "/home/worker/keys/${SIGNING_CERT}.pubkey" \
    $EXTRA_PARAMS
