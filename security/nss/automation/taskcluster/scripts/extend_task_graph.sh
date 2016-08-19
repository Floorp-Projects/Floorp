#!/usr/bin/env bash

set -v -e -x

if [ $(id -u) = 0 ]; then
    # Drop privileges by re-running this script.
    exec su worker $0
fi

mkdir -p /home/worker/artifacts

# Install Node.JS dependencies.
cd nss/automation/taskcluster/graph/ && npm install

# Build the task graph definition.
nodejs build.js > /home/worker/artifacts/graph.json
