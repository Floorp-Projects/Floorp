#!/usr/bin/env bash

set -v -e -x

if [[ $(id -u) -eq 0 ]]; then
    # Stupid Docker. It works without sometimes... But not always.
    echo "127.0.0.1 localhost.localdomain" >> /etc/hosts

    # Drop privileges by re-running this script.
    # Note: this mangles arguments, better to avoid running scripts as root.
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
