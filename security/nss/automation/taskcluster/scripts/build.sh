#!/usr/bin/env bash

set -v -e -x

if [ $(id -u) = 0 ]; then
    # Switch compilers.
    GCC=${GCC_VERSION:-gcc-5}
    GXX=${GXX_VERSION:-g++-5}

    update-alternatives --set gcc "/usr/bin/$GCC"
    update-alternatives --set g++ "/usr/bin/$GXX"

    # Drop privileges by re-running this script.
    exec su worker $0
fi

# Clone NSPR if needed.
if [ ! -d "nspr" ]; then
    hg clone https://hg.mozilla.org/projects/nspr
fi

# Build.
cd nss && make nss_build_all

# Generate certificates.
cd tests && NSS_TESTS=cert NSS_CYCLES="standard pkix sharedb" ./all.sh

# Reset test counter so that test runs pick up our certificates.
cd && echo 1 > tests_results/security/localhost

# Package.
mkdir artifacts
tar cvfjh artifacts/dist.tar.bz2 dist tests_results
