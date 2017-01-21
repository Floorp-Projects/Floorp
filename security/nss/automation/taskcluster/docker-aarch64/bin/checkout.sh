#!/usr/bin/env bash

set -v -e -x

if [ $(id -u) = 0 ]; then
    # Drop privileges by re-running this script.
    exec su worker $0
fi

# Default values for testing.
REVISION=${NSS_HEAD_REVISION:-default}
REPOSITORY=${NSS_HEAD_REPOSITORY:-https://hg.mozilla.org/projects/nss}

# Clone NSS.
for i in 0 2 5; do
    sleep $i
    hg clone -r $REVISION $REPOSITORY nss && exit 0
    rm -rf nss
done
exit 1
