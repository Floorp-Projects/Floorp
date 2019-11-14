#!/usr/bin/env bash

set -v -e -x

# Set up the toolchain.
source $(dirname $0)/setup.sh

# Fetch artifact.
wget -t 3 --retry-connrefused -w 5 --random-wait https://queue.taskcluster.net/v1/task/$TC_PARENT_TASK_ID/artifacts/public/build/dist.7z -O dist.7z
7z x dist.7z

# Generate certificates.
NSS_TESTS=cert NSS_CYCLES="standard pkix sharedb" nss/tests/all.sh

# Reset test counter so that test runs pick up our certificates.
echo 1 > tests_results/security/localhost

# Package.
7z a public/build/dist.7z dist tests_results
