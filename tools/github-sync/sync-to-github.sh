#!/usr/bin/env bash

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/. */

# TODO(kats): consider moving this to Python

# Do NOT set -x here, since that will expose a secret API token!
set -o errexit
set -o nounset
set -o pipefail

if [[ "$(uname)" != "Linux" ]]; then
    echo "Error: this script must be run on Linux due to readlink semantics"
    exit 1
fi

# GECKO_PATH should definitely be set
if [[ -z "${GECKO_PATH}" ]]; then
    echo "Error: GECKO_PATH must point to a hg clone of mozilla-central"
    exit 1
fi

# Internal variables, don't fiddle with these
MYSELF=$(readlink -f ${0})
MYDIR=$(dirname "${MYSELF}")
WORKDIR="${HOME}/.ghsync"
TMPDIR="${WORKDIR}/tmp"

NAME="$1"
RELATIVE_PATH="$2"
DOWNSTREAM_REPO="$3"
BORS="$4"
BRANCH="github-sync"

mkdir -p "${TMPDIR}"

# Bring the project clone to a known good up-to-date state
if [[ ! -d "${WORKDIR}/${NAME}" ]]; then
    echo "Setting up ${NAME} repo..."
    git clone "https://github.com/${DOWNSTREAM_REPO}" "${WORKDIR}/${NAME}"
    pushd "${WORKDIR}/${NAME}"
    git remote add moz-gfx https://github.com/moz-gfx/${NAME}
    popd
else
    echo "Updating ${NAME} repo..."
    pushd "${WORKDIR}/${NAME}"
    git checkout master
    git pull
    popd
fi

if [[ -n "${GITHUB_SECRET:-}" ]]; then
    echo "Obtaining github API token..."
    # Be careful, GITHUB_TOKEN is secret, so don't log it (or any variables
    # built using it).
    GITHUB_TOKEN=$(
        curl -sSfL "$TASKCLUSTER_PROXY_URL/secrets/v1/secret/${GITHUB_SECRET}" |
        ${MYDIR}/read-json.py "secret/token"
    )
    AUTH="moz-gfx:${GITHUB_TOKEN}"
    CURL_AUTH="Authorization: bearer ${GITHUB_TOKEN}"
fi

echo "Pushing base ${BRANCH} branch..."
pushd "${WORKDIR}/${NAME}"
git fetch moz-gfx
git checkout -B ${BRANCH} moz-gfx/${BRANCH} || git checkout -B ${BRANCH} master

if [[ -n "${GITHUB_SECRET:-}" ]]; then
    # git may emit error messages that contain the URL, so let's sanitize them
    # or we might leak the auth token to the task log.
    git push "https://${AUTH}@github.com/moz-gfx/${NAME}" \
        "${BRANCH}:${BRANCH}" 2>&1 | sed -e "s/${AUTH}/_SANITIZED_/g"
fi
popd

# Run the converter
echo "Running converter..."
pushd "${GECKO_PATH}"
"${MYDIR}/converter.py" "${WORKDIR}/${NAME}" "${RELATIVE_PATH}"
popd

# Check to see if we have changes that need pushing
echo "Checking for new changes..."
pushd "${WORKDIR}/${NAME}"
PATCHCOUNT=$(git log --oneline moz-gfx/${BRANCH}..${BRANCH}| wc -l)
if [[ ${PATCHCOUNT} -eq 0 ]]; then
    echo "No new patches found, aborting..."
    exit 0
fi

# Log the new changes, just for logging purposes
echo "Here are the new changes:"
git log --graph --stat moz-gfx/${BRANCH}..${BRANCH}

# Collect PR numbers of PRs opened on Github and merged to m-c
set +e
FIXES=$(
    git log master..${BRANCH} |
    grep "\[import_pr\] From https://github.com/${DOWNSTREAM_REPO}/pull" |
    sed -e "s%.*pull/% Fixes #%" |
    uniq |
    tr '\n' ','
)
echo "${FIXES}"
set -e

if [[ -z "${GITHUB_SECRET:-}" ]]; then
    echo "Running in try push, exiting now"
    exit 0
fi

echo "Pushing new changes to moz-gfx..."
# git may emit error messages that contain the URL, so let's sanitize them
# or we might leak the auth token to the task log.
git push "https://${AUTH}@github.com/moz-gfx/${NAME}" +${BRANCH}:${BRANCH} \
    2>&1 | sed -e "s/${AUTH}/_SANITIZED_/g"

CURL_HEADER="Accept: application/vnd.github.v3+json"
CURL=(curl -sSfL -H "${CURL_HEADER}" -H "${CURL_AUTH}")
# URL extracted here mostly to make servo-tidy happy with line lengths
API_URL="https://api.github.com/repos/${DOWNSTREAM_REPO}"

# Check if there's an existing PR open
echo "Listing pre-existing pull requests..."
"${CURL[@]}" "${API_URL}/pulls?head=moz-gfx:${BRANCH}" |
    tee "${TMPDIR}/pr.get"
set +e
COMMENT_URL=$(cat "${TMPDIR}/pr.get" | ${MYDIR}/read-json.py "0/comments_url")
HAS_COMMENT_URL="${?}"
set -e

if [[ ${HAS_COMMENT_URL} -ne 0 ]]; then
    echo "Pull request not found, creating..."
    # The PR doesn't exist yet, so let's create it
    (   echo -n '{ "title": "Sync changes from mozilla-central '"${RELATIVE_PATH}"'"'
        echo -n ', "body": "'"${FIXES}"'"'
        echo -n ', "head": "moz-gfx:'"${BRANCH}"'"'
        echo -n ', "base": "master" }'
    ) > "${TMPDIR}/pr.create"
    "${CURL[@]}" -d "@${TMPDIR}/pr.create" "${API_URL}/pulls" |
        tee "${TMPDIR}/pr.response"
    COMMENT_URL=$(
        cat "${TMPDIR}/pr.response" |
        ${MYDIR}/read-json.py "comments_url"
    )
fi

# At this point COMMENTS_URL should be set, so leave a comment to tell bors
# to merge the PR.
echo "Posting r+ comment to ${COMMENT_URL}..."
echo '{ "body": "'"$BORS"' r=auto" }' > "${TMPDIR}/bors_rplus"
"${CURL[@]}" -d "@${TMPDIR}/bors_rplus" "${COMMENT_URL}"

echo "All done!"
