#!/usr/bin/env bash

source $(dirname "$0")/tools.sh

mkdir -p /home/worker/artifacts

# Install Node.JS dependencies.
cd nss/automation/taskcluster/graph/ && npm install

# Extend the task graph.
node lib/index.js
