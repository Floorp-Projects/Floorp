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
    arcanist \
    bzip2 \
    ca-certificates \
    curl \
    jq \
    libdbus-glib-1-2 \
    libgtk-3-0 \
    libxml2-utils \
    libxt6 \
    python \
    python3 \
    shellcheck \
    unzip \
    wget \

apt-get clean

. install-mercurial.sh

rm -rf /setup
