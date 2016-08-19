#!/usr/bin/env bash

set -v -e -x

# Set up the toolchain.
source $(dirname $0)/setup.sh

# Clone NSPR.
hg_clone https://hg.mozilla.org/projects/nspr nspr default

# Build.
make -C nss nss_build_all

# Generate certificates.
NSS_TESTS=cert NSS_CYCLES="standard pkix sharedb" nss/tests/all.sh

# Reset test counter so that test runs pick up our certificates.
echo 1 > tests_results/security/localhost

# Package.
7z a public/build/dist.7z dist tests_results
