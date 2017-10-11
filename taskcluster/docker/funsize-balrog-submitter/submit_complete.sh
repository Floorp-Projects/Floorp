#!/bin/bash

set -xe

test $PARENT_TASK_ARTIFACTS_URL_PREFIX
test $BALROG_API_ROOT
# BALROG_BLOB_SUFFIX is used by the script implicitly to avoid possible CLI
# issues with suffixes starting with "-"
test $BALROG_BLOB_SUFFIX


ARTIFACTS_DIR="/home/worker/artifacts"
mkdir -p "$ARTIFACTS_DIR"

curl --location --retry 10 --retry-delay 10 -o "$ARTIFACTS_DIR/manifest.json" \
    "$PARENT_TASK_ARTIFACTS_URL_PREFIX/manifest.json"

cat "$ARTIFACTS_DIR/manifest.json"
python /home/worker/bin/funsize-balrog-submitter-complete.py  \
    --manifest "$ARTIFACTS_DIR/manifest.json" \
    -a "$BALROG_API_ROOT" \
    --verbose \
    $EXTRA_BALROG_SUBMITTER_PARAMS
