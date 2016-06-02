#!/usr/bin/env bash

set -v -e -x

if [ $(id -u) = 0 ]; then
    # Drop privileges by re-running this script.
    exec su worker $0
fi

mkdir -p /home/worker/artifacts

# Build the task graph definition.
nodejs nss/automation/taskcluster/graph/build.js > /home/worker/artifacts/graph.json
