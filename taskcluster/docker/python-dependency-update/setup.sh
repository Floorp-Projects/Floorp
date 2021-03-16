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
    curl \
    gcc \
    jq \
    libdpkg-perl \
    liblzma-dev \
    python \
    python-dev \
    python-pip \
    python3.8 \
    python3.8-dev \
    python3-setuptools \
    python3-wheel \
    python3-pip

apt-get clean

. install-mercurial.sh
pip install -r requirements.txt

rm -rf /setup
