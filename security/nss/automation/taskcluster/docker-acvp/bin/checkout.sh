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
hg clone -r $REVISION $REPOSITORY nss

# Clone NSPR if needed.
hg clone -r default https://hg.mozilla.org/projects/nspr

if [[ -f nss/nspr.patch && "$ALLOW_NSPR_PATCH" == "1" ]]; then
  pushd nspr
  cat ../nss/nspr.patch | patch -p1
  popd
fi

