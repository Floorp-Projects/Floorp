#!/usr/bin/env bash

set -v -e -x

source $(dirname $0)/tools.sh

if [ $(id -u) = 0 ]; then
    # Set compiler.
    switch_compilers

    # Stupid Docker.
    echo "127.0.0.1 localhost.localdomain" >> /etc/hosts

    # Drop privileges by re-running this script.
    exec su worker $0
fi

# Fetch artifact if needed.
fetch_dist

# Generate certificates.
NSS_TESTS=cert NSS_CYCLES="standard pkix sharedb" $(dirname $0)/run_tests.sh

# Reset test counter so that test runs pick up our certificates.
echo 1 > tests_results/security/localhost

# Package.
mkdir artifacts
tar cvfjh artifacts/dist.tar.bz2 dist tests_results
