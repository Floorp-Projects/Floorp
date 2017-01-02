#!/usr/bin/env bash

set -v -e -x

if [ $(id -u) = 0 ]; then
    # set up fake uname
    if [ ! -f /bin/uname-real ]; then
        mv /bin/uname /bin/uname-real
        ln -s /home/worker/bin/uname.sh /bin/uname
    fi
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
