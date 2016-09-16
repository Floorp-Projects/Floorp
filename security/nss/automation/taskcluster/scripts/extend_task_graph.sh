#!/usr/bin/env bash

set -v -e -x

if [ $(id -u) = 0 ]; then
    # Drop privileges by re-running this script.
    exec su worker $0
fi

mkdir -p /home/worker/artifacts

# Install Node.JS dependencies.
cd nss/automation/taskcluster/graph/ && npm install

# Extend the task graph.
node lib/index.js
