#!/usr/bin/env bash

set -v -e -x

# Set up the toolchain.
source $(dirname $0)/setup.sh

# Fetch artifact.
if [ "$TASKCLUSTER_ROOT_URL" = "https://taskcluster.net" ] || [ -z "$TASKCLUSTER_ROOT_URL" ]; then
    url=https://queue.taskcluster.net/v1/task/$TC_PARENT_TASK_ID/artifacts/public/build/dist.7z
else
    url=$TASKCLUSTER_ROOT_URL/api/queue/v1/task/$TC_PARENT_TASK_ID/artifacts/public/build/dist.7z
fi

wget -t 3 --retry-connrefused -w 5 --random-wait $url -O dist.7z
7z x dist.7z

# Generate certificates.
NSS_TESTS=cert NSS_CYCLES="standard pkix sharedb" nss/tests/all.sh

# Reset test counter so that test runs pick up our certificates.
echo 1 > tests_results/security/localhost

# Package.
7z a public/build/dist.7z dist tests_results
