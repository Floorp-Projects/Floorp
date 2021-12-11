#!/usr/bin/env bash

source $(dirname "$0")/tools.sh

# Fetch artifact if needed.
fetch_dist

# Generate certificates.
NSS_TESTS=cert NSS_CYCLES="standard pkix sharedb" $(dirname $0)/run_tests.sh

# Reset test counter so that test runs pick up our certificates.
echo 1 > tests_results/security/localhost

# Package.
if [[ $(uname) = "Darwin" ]]; then
  mkdir -p public
  tar cvfjh public/dist.tar.bz2 dist tests_results
else
  mkdir artifacts
  tar cvfjh artifacts/dist.tar.bz2 dist tests_results
fi
