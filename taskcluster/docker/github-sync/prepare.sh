#!/usr/bin/env bash

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/. */

set -o errexit
set -o nounset
set -o pipefail
set -o xtrace

test "$(whoami)" == 'root'

# Install stuff we need
apt-get -y update
apt-get install -y \
    cmake \
    curl \
    gcc \
    git \
    g++ \
    libffi-dev \
    libgit2-dev \
    libssl-dev \
    python3 \
    python3-dev \
    python3-pip \
    python3-setuptools

# Python packages
pip3 install --break-system-packages -r "$1"
