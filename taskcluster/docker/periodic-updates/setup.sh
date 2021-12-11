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
    libdbus-glib-1-2 \
    libgtk-3-0 \
    libx11-xcb1 \
    libxml2-utils \
    libxt6 \
    python \
    shellcheck \
    unzip \
    wget

rm -rf /setup
