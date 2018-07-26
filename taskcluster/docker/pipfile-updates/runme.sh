#!/bin/bash

set -xe

# Things to be set by task definition.
# -b branch
# -p pipfile_directory
# -3 use python3


test "${BRANCH}"
test "${PIPFILE_DIRECTORY}"

PIP_ARG="-2"
if [ -n "${PYTHON3}" ]; then
  PIP_ARG="-3"
fi

export ARTIFACTS_DIR="/home/worker/artifacts"
mkdir -p "$ARTIFACTS_DIR"

# Get Arcanist API token

if [ -n "${TASK_ID}" ]
then
  curl --location --retry 10 --retry-delay 10 -o /home/worker/task.json \
    "https://queue.taskcluster.net/v1/task/$TASK_ID"
  ARC_SECRET=$(jq -r '.scopes[] | select(contains ("arc-phabricator-token"))' /home/worker/task.json | awk -F: '{print $3}')
fi
if [ -n "${ARC_SECRET}" ] && getent hosts taskcluster
then
  set +x # Don't echo these
  secrets_url="http://taskcluster/secrets/v1/secret/${ARC_SECRET}"
  SECRET=$(curl "${secrets_url}")
  TOKEN=$(echo "${SECRET}" | jq -r '.secret.token')
elif [ -n "${ARC_TOKEN}" ] # Allow for local testing.
then
  TOKEN="${ARC_TOKEN}"
fi

if [ -n "${TOKEN}" ]
then
  cat >"${HOME}/.arcrc" <<END
{
  "hosts": {
    "https://phabricator.services.mozilla.com/api/": {
      "token": "${TOKEN}"
    }
  }
}
END
  set -x
  chmod 600 "${HOME}/.arcrc"
fi

# shellcheck disable=SC2086
/home/worker/scripts/update_pipfiles.sh -b "${BRANCH}" -p "${PIPFILE_DIRECTORY}" ${PIP_ARG}
