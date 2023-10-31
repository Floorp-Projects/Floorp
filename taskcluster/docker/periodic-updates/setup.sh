#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set -ve

apt-get update -q
apt-get install \
    arcanist \
    curl \
    jq \
    libasound2 \
    libgtk-3-0 \
    libx11-xcb1 \
    libxml2-utils \
    libxt6 \
    libxtst6 \
    shellcheck \
    unzip \
    bzip2 \
    wget

rm -rf /setup
