#!/usr/bin/env bash

set -v -e -x

# Assert that we're not running as root.
if [[ $(id -u) -eq 0 ]]; then
    # This exec is still needed until aarch64 images are updated (Bug 1488325).
    # Remove when images are updated.  Until then, assert that things are good.
    [[ $(uname -m) == aarch64 ]]
    exec su worker -c "$0 $*"
fi

export PATH="${PATH}:/home/worker/.cargo/bin/:/usr/lib/go-1.6/bin"

# Usage: hg_clone repo dir [revision=@]
hg_clone() {
    repo=$1
    dir=$2
    rev=${3:-@}
    if [ -d "$dir" ]; then
        hg pull -R "$dir" -ur "$rev" "$repo" && return
        rm -rf "$dir"
    fi
    for i in 0 2 5; do
        sleep $i
        hg clone -r "$rev" "$repo" "$dir" && return
        rm -rf "$dir"
    done
    exit 1
}

fetch_dist() {
    url=https://queue.taskcluster.net/v1/task/$TC_PARENT_TASK_ID/artifacts/public/dist.tar.bz2
    if [ ! -d "dist" ]; then
        for i in 0 2 5; do
            sleep $i
            curl --retry 3 -Lo dist.tar.bz2 $url && tar xvjf dist.tar.bz2 && return
            rm -fr dist.tar.bz2 dist
        done
        exit 1
    fi
}
