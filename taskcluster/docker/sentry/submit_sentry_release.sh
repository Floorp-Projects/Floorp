#!/bin/bash

set -o nounset
set -o pipefail

run() {
    revisions=$(curl "$HG_PUSHLOG_URL" | jq -c -r ".pushes[].changesets | @sh" | tr -d \') || return 1
    sentry_api_key=$(curl "http://taskcluster/secrets/v1/secret/project/engwf/gecko/3/tokens" | jq -r ".secret.sentryToken") || return 1
    for revision in $revisions; do
        SENTRY_API_KEY=$sentry_api_key SENTRY_ORG=operations sentry-cli --url https://sentry.prod.mozaws.net/ releases --project mach new "$revision" || return 1
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
