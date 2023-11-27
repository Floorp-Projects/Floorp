#!/bin/bash

set -ex

TASKCLUSTER_ROOT_URL=https://firefox-ci-tc.services.mozilla.com

dir=$(dirname "$0")
if [ -n "$1" ]; then
    files="$@"
else
    files=$(ls -1 "$dir"/*.yml)
fi
for f in $files; do
    base=$(basename "$f" .yml)
    repo=${base%%-*}
    action=${base#*-}
    # remove people's email addresses
    filter='.owner="user@example.com"'

    case $repo in
        mc)
            repo=mozilla-central
            ;;
        mb)
            repo=mozilla-beta
            ;;
        mr)
            repo=mozilla-release
            ;;
        me)
            version=$(curl -s https://product-details.mozilla.org/1.0/firefox_versions.json | jq -r  .FIREFOX_ESR)
            version=${version%%.*}
            repo=mozilla-esr${version}
            # unset enable_always_target to fall back to the default, to avoid
            # generating a broken graph with esr115 params
            filter="$filter | del(.enable_always_target)"
            ;;
        autoland)
            ;;
        try)
            continue
            ;;
        *)
            echo unknown repo $repo >&2
            exit 1
            ;;
    esac

    case $action in
        onpush)
            task=gecko.v2.${repo}.latest.taskgraph.decision
            service=index
            # find a non-DONTBUILD push
            while :; do
                params=$(curl -f -L ${TASKCLUSTER_ROOT_URL}/api/${service}/v1/task/${task}/artifacts/public%2Fparameters.yml)
                method=$(echo "$params" | yq -r .target_tasks_method)
                if [ $method != nothing ]; then
                    break
                fi
                pushlog_id=$(echo "$params" | yq -r .pushlog_id)
                task=gecko.v2.${repo}.pushlog-id.$((pushlog_id - 1)).decision
            done
            ;;
        onpush-geckoview)
            # this one is weird, ignore it
            continue
            ;;
        cron-*)
            task=${action#cron-}
            task=gecko.v2.${repo}.latest.taskgraph.decision-${task}
            service=index
            ;;
        desktop-nightly)
            task=gecko.v2.${repo}.latest.taskgraph.decision-nightly-desktop
            service=index
            ;;
        ship-geckoview)
            task=gecko.v2.${repo}.latest.taskgraph.decision-ship-geckoview
            service=index
            ;;
        push*|promote*|ship*)
            case $action in
                *-partials)
                    action=${action%-partials}
                    ;;
                *)
                    filter="$filter | .release_history={}"
                    ;;
            esac
            suffix=
            case $action in
                *-firefox-rc)
                    product=firefox
                    action=${action%-firefox-rc}
                    suffix=_rc
                    ;;
                *-firefox)
                    product=firefox
                    action=${action%-$product}
                    ;;
                *-devedition)
                    product=devedition
                    action=${action%-$product}
                    ;;
                *)
                    echo unknown action $action >&2
                    exit 1
                    ;;
            esac
            phase=${action}_${product}${suffix}
            # grab the action task id from the latest release where this phase wasn't skipped
            task=$(curl -s "https://shipitapi-public.services.mozilla.com/releases?product=${product}&branch=releases/${repo}&status=shipped" | \
                jq -r "map(.phases[] | select(.name == "'"'"$phase"'"'" and (.skipped | not)))[-1].actionTaskId")
            service=queue
            ;;
        *)
            echo unknown action $action >&2
            exit 1
            ;;
    esac

    curl -f -L ${TASKCLUSTER_ROOT_URL}/api/${service}/v1/task/${task}/artifacts/public%2Fparameters.yml | yq -y "$filter" > "${f}"
done
