#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set -ve

tooltool_fetch() {
    cat >manifest.tt
    python2.7 /setup/tooltool.py fetch
    rm manifest.tt
}

useradd -d /home/worker -s /bin/bash -m worker

apt-get update -q
apt-get install -y --no-install-recommends \
    arcanist=0~git20170812-1 \
    bzip2=1.0.6-8.1 \
    ca-certificates \
    curl \
    jq=1.5+dfsg-2 \
    libdbus-glib-1-2=0.110-2 \
    libgtk-3-0=3.22.28-1ubuntu3 \
    libxml2-utils=2.9.4+dfsg1-6.1ubuntu1 \
    libxt6=1:1.1.5-1 \
    python \
    python3=3.6.4-1 \
    shellcheck=0.4.6-1 \
    unzip=6.0-21ubuntu1 \
    wget=1.19.4-1ubuntu2 \

apt-get clean

. install-mercurial.sh

rm -rf /setup
