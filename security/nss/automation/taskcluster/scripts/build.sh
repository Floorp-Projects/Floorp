#!/usr/bin/env bash

set -v -e -x

source $(dirname $0)/tools.sh

if [[ $(id -u) -eq 0 ]]; then
    # Set compiler.
    switch_compilers

    # Drop privileges by re-running this script.
    exec su worker $0
fi

# Clone NSPR if needed.
hg_clone https://hg.mozilla.org/projects/nspr nspr default

# Build.
make -C nss nss_build_all

# Generate certificates.
NSS_TESTS=cert NSS_CYCLES="standard pkix sharedb" $(dirname $0)/run_tests.sh

# Reset test counter so that test runs pick up our certificates.
echo 1 > tests_results/security/localhost

# Package.
mkdir artifacts
tar cvfjh artifacts/dist.tar.bz2 dist tests_results
