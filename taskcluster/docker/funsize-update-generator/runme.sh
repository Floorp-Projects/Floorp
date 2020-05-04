#!/bin/sh

set -xe

test "$TASK_ID"
test "$SIGNING_CERT"

ARTIFACTS_DIR="/home/worker/artifacts"
mkdir -p "$ARTIFACTS_DIR"

# duplicate the functionality of taskcluster-lib-urls, but in bash..
queue_base="${TASKCLUSTER_ROOT_URL%/}/api/queue/v1"

curl --location --retry 10 --retry-delay 10 -o /home/worker/task.json "$queue_base/task/$TASK_ID"

# auth:aws-s3:read-write:tc-gp-private-1d-us-east-1/releng/mbsdiff-cache/
# -> bucket of tc-gp-private-1d-us-east-1, path of releng/mbsdiff-cache/
# Trailing slash is important, due to prefix permissions in S3.
S3_BUCKET_AND_PATH=$(jq -r '.scopes[] | select(contains ("auth:aws-s3"))' /home/worker/task.json | awk -F: '{print $4}')

# Will be empty if there's no scope for AWS S3.
if [ -n "${S3_BUCKET_AND_PATH}" ] && getent hosts taskcluster
then
  # Does this parse as we expect?
  S3_PATH=${S3_BUCKET_AND_PATH#*/}
  AWS_BUCKET_NAME=${S3_BUCKET_AND_PATH%/${S3_PATH}*}
  test "${S3_PATH}"
  test "${AWS_BUCKET_NAME}"

  set +x  # Don't echo these.
  secret_url="${TASKCLUSTER_PROXY_URL}/api/auth/v1/aws/s3/read-write/${AWS_BUCKET_NAME}/${S3_PATH}"
  AUTH=$(curl "${secret_url}")
  AWS_ACCESS_KEY_ID=$(echo "${AUTH}" | jq -r '.credentials.accessKeyId')
  AWS_SECRET_ACCESS_KEY=$(echo "${AUTH}" | jq -r '.credentials.secretAccessKey')
  AWS_SESSION_TOKEN=$(echo "${AUTH}" | jq -r '.credentials.sessionToken')
  export AWS_ACCESS_KEY_ID
  export AWS_SECRET_ACCESS_KEY
  export AWS_SESSION_TOKEN
  AUTH=

  if [ -n "$AWS_ACCESS_KEY_ID" ] && [ -n "$AWS_SECRET_ACCESS_KEY" ]; then
    # Pass the full bucket/path prefix, as the script just appends local files.
    export MBSDIFF_HOOK="/home/worker/bin/mbsdiff_hook.sh -S ${S3_BUCKET_AND_PATH}"
  fi
  set -x
else
  # disable caching
  export MBSDIFF_HOOK=
fi

# EXTRA_PARAMS is optional
# shellcheck disable=SC2086
python3.8 /home/worker/bin/funsize.py \
    --artifacts-dir "$ARTIFACTS_DIR" \
    --task-definition /home/worker/task.json \
    --signing-cert "/home/worker/keys/${SIGNING_CERT}.pubkey" \
    $EXTRA_PARAMS
