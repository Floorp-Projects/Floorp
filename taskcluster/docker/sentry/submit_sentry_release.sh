#!/bin/bash

set -o nounset
set -o pipefail

run() {
    revisions=$(curl "$HG_PUSHLOG_URL" | jq -c -r ".pushes[].changesets | @sh" | tr -d \') || return 1
    sentry_api_key=$(curl "http://taskcluster/secrets/v1/secret/$SENTRY_SECRET" | jq -r ".secret.sentryToken") || return 1
    for revision in $revisions; do
        SENTRY_AUTH_TOKEN=$sentry_api_key SENTRY_ORG=mozilla sentry-cli --url https://sentry.io/ releases --project mach new "hg-rev-$revision" || return 1
    done
}

with_backoff() {
    failures=0
    while ! "$@"; do
        failures=$(( failures + 1 ))
        if (( failures >= 5 )); then
            echo "[with_backoff] Unable to succeed after 5 tries, failing the job."
            return 1
        else
            seconds=$(( 2 ** (failures - 1) ))
            echo "[with_backoff] Retrying in $seconds second(s)"
            sleep $seconds
        fi
    done
}

with_backoff run
