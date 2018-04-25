#!/usr/bin/env bash

set -v -e

test "$(whoami)" == 'root'

apt-get update
apt-get install -y --force-yes --no-install-recommends \
    ca-certificates \
    python \
    sudo

BUILD=/root/build
mkdir "$BUILD"

tooltool_fetch() {
    cat >manifest.tt
    python2.7 /tmp/tooltool.py fetch
    rm manifest.tt
}

cd $BUILD
# shellcheck disable=SC1091
. /tmp/install-mercurial.sh

cd /
rm -rf $BUILD
apt-get clean
apt-get autoclean
rm -rf /var/lib/apt/lists/
rm "$0"
