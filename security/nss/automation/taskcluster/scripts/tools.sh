#!/usr/bin/env bash

set -v -e -x

switch_compilers() {
    GCC=`which ${GCC_VERSION:-gcc-5}`
    GXX=`which ${GXX_VERSION:-g++-5}`

    if [ -e "$GCC" ] && [ -e "$GXX" ]; then
        update-alternatives --set gcc $GCC
        update-alternatives --set g++ $GXX
    else
        echo "Unknown compiler $GCC_VERSION/$GXX_VERSION."
        exit 1
    fi
}

# Usage: hg_clone repo dir [revision=@]
hg_clone() {
    repo=$1
    dir=$2
    rev=${3:-@}
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
