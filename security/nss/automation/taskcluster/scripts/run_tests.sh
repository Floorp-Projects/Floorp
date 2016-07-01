#!/usr/bin/env bash

set -v -e -x

if [ $(id -u) = 0 ]; then
    source $(dirname $0)/tools.sh

    # Set compiler.
    switch_compilers

    # Stupid Docker.
    echo "127.0.0.1 localhost.localdomain" >> /etc/hosts

    # Drop privileges by re-running this script.
    exec su worker $0
fi

# Fetch artifact if needed.
if [ ! -d "dist" ]; then
    curl --retry 3 -Lo dist.tar.bz2 https://queue.taskcluster.net/v1/task/$TC_PARENT_TASK_ID/artifacts/public/dist.tar.bz2
    tar xvjf dist.tar.bz2
fi

# Run tests.
cd nss/tests && ./all.sh
